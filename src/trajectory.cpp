#include "trajectory.h"
#include "spline.h"
#include "utility.h"
#include "map.h"
#include "params.h"

#include "Eigen-3.3/Eigen/Dense"

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

// 50 x {s, s_dot, s_ddot}
static vector<vector<double>> previous_path_s(param_nb_points, {0, 0, 0});
static vector<vector<double>> previous_path_d(param_nb_points, {0, 0, 0});

void JMT_init(double car_s, double car_d)
{
  previous_path_s[0] = { car_s, 0, 0};
  previous_path_d[0] = { car_d, 0, 0};
}
  

vector<double> JMT(vector< double> start, vector <double> end, double T)
{
    /*
    Calculate the Jerk Minimizing Trajectory that connects the initial state
    to the final state in time T.

    INPUTS

    start - the vehicles start location given as a length three array
        corresponding to initial values of [s, s_dot, s_double_dot]

    end   - the desired end state for vehicle. Like "start" this is a
        length three array.

    T     - The duration, in seconds, over which this maneuver should occur.

    OUTPUT 
    an array of length 6, each value corresponding to a coefficent in the polynomial 
    s(t) = a_0 + a_1 * t + a_2 * t**2 + a_3 * t**3 + a_4 * t**4 + a_5 * t**5

    EXAMPLE

    > JMT( [0, 10, 0], [10, 10, 0], 1)
    [0.0, 10.0, 0.0, 0.0, 0.0, 0.0]
    */

    MatrixXd A(3,3);
    VectorXd b(3);
    VectorXd x(3);

    A <<   pow(T,3),    pow(T,4),    pow(T,5),
         3*pow(T,2),  4*pow(T,3),  5*pow(T,4),
                6*T, 12*pow(T,2), 20*pow(T,3);

    b << end[0] - (start[0] + start[1]*T + 0.5*start[2]*T*T), 
         end[1] - (start[1] + start[2]*T), 
         end[2] - start[2];

    x = A.inverse() * b;

    return {start[0], start[1], start[2]/2, x[0], x[1], x[2]};
}


// c: coefficients of polynom
double polyeval(vector<double> c, double t)
{
  double res = 0.0;
  for (int i = 0; i < c.size(); i++)
  {
    res += c[i] * pow(t, i);
  }
  return res;
}

// 1st derivative of a polynom
double polyeval_dot(vector<double> c, double t) {
  double res = 0.0;
  for (int i = 1; i < c.size(); ++i) {
    res += i * c[i] * pow(t, i-1);
  }
  return res;
}

// 2nd derivative of a polynom
double polyeval_ddot(vector<double> c, double t) {
  double res = 0.0;
  for (int i = 2; i < c.size(); ++i) {
    res += i * (i-1) * c[i] * pow(t, i-2);
  }
  return res;
}



vector<vector<double>> generate_trajectory_jmt(int target_lane, double target_vel, Map &map, double car_x, double car_y, double car_yaw, double car_s, double car_d, vector<double> previous_path_x, vector<double> previous_path_y)
{

  int prev_size = previous_path_x.size();
  int nb_points_used = param_nb_points - prev_size;
  int last_point = nb_points_used -1;

  assert( car_s == previous_path_s[last_point][0] );
  assert( car_d == previous_path_d[last_point][0] );

  /////////////////////////////////////////////////////////////
  // TODO compute sf, sf_dot and T

  double T = 2; // 2 seconds si car_d center of line

  // si si_dot si_ddot: to be retieved
  double si = previous_path_s[last_point][0];
  double si_dot = previous_path_s[last_point][1];
  double si_ddot = previous_path_s[last_point][2];

  double di = previous_path_d[last_point][0];
  double di_dot = previous_path_d[last_point][1];
  double di_ddot = previous_path_d[last_point][2];


  // sf sfdot 0

  vector<double> start_s = { si, si_dot, si_ddot}; // si si_dot si_ddot
  vector<double> end_s = { car_s, 0, 0};   // sf sf_dot 0

  vector<double> start_d = { di, di_dot, di_ddot }; // di di_dot di_ddot
  vector<double> end_d = { get_dcenter(target_lane), 0, 0}; // df 0 0

  /////////////////////////////////////////////////////////////

  vector<double> poly_s = JMT(start_s, end_s, T);
  vector<double> poly_d = JMT(start_d, end_d, T);

  vector<double> next_x_vals;
  vector<double> next_y_vals;
  
  for (int i = 0; i < prev_size; i++)
  {
    previous_path_s[i] = previous_path_s[param_nb_points - prev_size + i];
    previous_path_d[i] = previous_path_d[param_nb_points - prev_size + i];

    next_x_vals.push_back(previous_path_x[i]);
    next_y_vals.push_back(previous_path_y[i]);
  }

  double t = 0.0;
  for (int i = prev_size; i < param_nb_points; i++)
  {
    double s = polyeval(poly_s, t);
    double s_dot = polyeval_dot(poly_s, t);
    double s_ddot = polyeval_ddot(poly_s, t);

    double d = polyeval(poly_d, t);
    double d_dot = polyeval_dot(poly_d, t);
    double d_ddot = polyeval_ddot(poly_d, t);

    previous_path_s[i] = { s, s_dot, s_ddot };
    previous_path_d[i] = { d, d_dot, d_ddot };

    vector<double> point_xy = map.getXYspline(s, d);

    next_x_vals.push_back(point_xy[0]);
    next_y_vals.push_back(point_xy[1]);

    t += param_dt;
  }

  return { next_x_vals, next_y_vals };
}



vector<vector<double>> generate_trajectory(int target_lane, double target_vel, Map &map, double car_x, double car_y, double car_yaw, double car_s, double car_d, vector<double> previous_path_x, vector<double> previous_path_y)
{
  vector<double> ptsx;
  vector<double> ptsy;
  
  double ref_x = car_x;
  double ref_y = car_y;
  double ref_yaw = deg2rad(car_yaw);

  int prev_size = previous_path_x.size();
  
  if (prev_size < 2)
  {
    double prev_car_x = car_x - cos(car_yaw);
    double prev_car_y = car_y - sin(car_yaw);
  
    ptsx.push_back(prev_car_x);
    ptsx.push_back(car_x);
  
    ptsy.push_back(prev_car_y);
    ptsy.push_back(car_y);
  }
  else
  {
    ref_x = previous_path_x[prev_size-1];
    ref_y = previous_path_y[prev_size-1];
  
    double ref_x_prev = previous_path_x[prev_size-2];
    double ref_y_prev = previous_path_y[prev_size-2];
    ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);
  
    ptsx.push_back(ref_x_prev);
    ptsx.push_back(ref_x);
  
    ptsy.push_back(ref_y_prev);
    ptsy.push_back(ref_y);
  }
  
  vector<double> next_wp0 = map.getXY(car_s+30, get_dcenter(target_lane));
  vector<double> next_wp1 = map.getXY(car_s+60, get_dcenter(target_lane));
  vector<double> next_wp2 = map.getXY(car_s+90, get_dcenter(target_lane));
  
  
  ptsx.push_back(next_wp0[0]);
  ptsx.push_back(next_wp1[0]);
  ptsx.push_back(next_wp2[0]);
  
  ptsy.push_back(next_wp0[1]);
  ptsy.push_back(next_wp1[1]);
  ptsy.push_back(next_wp2[1]);
  
  
  for (int i = 0; i < ptsx.size(); i++)
  {
    // shift car reference angle to 0 degrees
    // transformation to local car's coordinates (cf MPC)
    // last point of previous path at origin and its angle at zero degree
  
    // shift and rotation
    double shift_x = ptsx[i]-ref_x;
    double shift_y = ptsy[i]-ref_y;
  
    ptsx[i] = (shift_x * cos(0-ref_yaw) - shift_y * sin(0 - ref_yaw));
    ptsy[i] = (shift_x * sin(0-ref_yaw) + shift_y * cos(0 - ref_yaw));
  }
  
  
  tk::spline spl;
  spl.set_points(ptsx, ptsy);
  
  vector<double> next_x_vals;
  vector<double> next_y_vals;
  
  for (int i = 0; i < prev_size; i++)
  {
    next_x_vals.push_back(previous_path_x[i]);
    next_y_vals.push_back(previous_path_y[i]);
  }
  
  // Calculate how to break up spline points so that we travel at our desired reference velocity
  double target_x = 30.0;
  double target_y = spl(target_x);
  double target_dist = sqrt(target_x*target_x + target_y*target_y);
  
  double x_add_on = 0;
  
  // fill up the rest of our path planner after filing it with previous points
  // here we will always output 50 points
  for (int i = 1; i <= param_nb_points - prev_size; i++)
  {
    double N = (target_dist / (param_dt * mph_to_ms(target_vel))); // divide by 2.24: mph -> m/s
    double x_point = x_add_on + target_x/N;
    double y_point = spl(x_point);
  
    x_add_on = x_point;
  
    double x_ref = x_point; // x_ref IS NOT ref_x !!!
    double y_ref = y_point;
  
    // rotate back to normal after rotating it earlier
    x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
    y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));
  
    x_point += ref_x;
    y_point += ref_y;
  
    next_x_vals.push_back(x_point);
    next_y_vals.push_back(y_point);
  }

  return { next_x_vals, next_y_vals };
}
