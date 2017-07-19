// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_view.h"

#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_segment_utils.h"
#include "base/containers/adapters.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Variables for rendering the laser. Radius in DIP.
const float kPointInitialRadius = 5.0f;
const float kPointFinalRadius = 0.25f;
const int kPointInitialOpacity = 200;
const int kPointFinalOpacity = 10;
const SkColor kPointColor = SkColorSetRGB(255, 0, 0);
// Change this when debugging prediction code.
const SkColor kPredictionPointColor = kPointColor;

float DistanceBetweenPoints(const gfx::PointF& point1,
                            const gfx::PointF& point2) {
  return (point1 - point2).Length();
}

float LinearInterpolate(float initial_value,
                        float final_value,
                        float progress) {
  return initial_value + (final_value - initial_value) * progress;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// The laser segment calcuates the path needed to draw a laser segment. A laser
// segment is used instead of just a regular line segments to avoid overlapping.
// A laser segment looks as follows:
//    _______         _________       _________        _________
//   /       \        \       /      /         /      /         \       |
//   |   A   |       2|.  B  .|1    2|.   C   .|1    2|.   D     \.1    |
//   |       |        |       |      |         |      |          /      |
//    \_____/         /_______\      \_________\      \_________/       |
//
//
// Given a start and end point (represented by the periods in the above
// diagrams), we create each segment by projecting each point along the normal
// to the line segment formed by the start(1) and end(2) points. We then
// create a path using arcs and lines. There are three types of laser segments:
// head(B), regular(C) and tail(D). A typical laser is created by rendering one
// tail(D), zero or more regular segments(C), one head(B) and a circle at the
// end(A). They are meant to fit perfectly with the previous and next segments,
// so that no whitespace/overlap is shown.
// A more detailed version of this is located at https://goo.gl/qixdux.
class LaserSegment {
 public:
  LaserSegment(const std::vector<gfx::PointF>& previous_points,
               const gfx::PointF& start_point,
               const gfx::PointF& end_point,
               float start_radius,
               float end_radius,
               bool is_last_segment) {
    DCHECK(previous_points.empty() || previous_points.size() == 2u);
    bool is_first_segment = previous_points.empty();

    // Calculate the variables for the equation of the lines which pass through
    // the start and end points, and are perpendicular to the line segment
    // between the start and end points.
    float slope, start_y_intercept, end_y_intercept;
    ComputeNormalLineVariables(start_point, end_point, &slope,
                               &start_y_intercept, &end_y_intercept);

    // Project the points along normal line by the given radius.
    gfx::PointF end_first_projection, end_second_projection;
    ComputeProjectedPoints(end_point, slope, end_y_intercept, end_radius,
                           &end_first_projection, &end_second_projection);

    // Create a collection of the points used to create the path and reorder
    // them as needed.
    std::vector<gfx::PointF> ordered_points;
    ordered_points.reserve(4);
    if (!is_first_segment) {
      ordered_points.push_back(previous_points[1]);
      ordered_points.push_back(previous_points[0]);
    } else {
      // We push two of the same point, so that for both cases we have 4 points,
      // and we can use the same indexes when creating the path.
      ordered_points.push_back(start_point);
      ordered_points.push_back(start_point);
    }
    // Push the projected points so that the the smaller angle relative to the
    // line segment between the two data points is first. This will ensure there
    // is always a anticlockwise arc between the last two points, and always a
    // clockwise arc for these two points if and when they are used in the next
    // segment.
    if (IsFirstPointSmallerAngle(start_point, end_point, end_first_projection,
                                 end_second_projection)) {
      ordered_points.push_back(end_first_projection);
      ordered_points.push_back(end_second_projection);
    } else {
      ordered_points.push_back(end_second_projection);
      ordered_points.push_back(end_first_projection);
    }

    // Create the path. The path always goes as follows:
    // 1. Move to point 0.
    // 2. Arc clockwise from point 0 to point 1. This step is skipped if it
    //    is the tail segment.
    // 3. Line from point 1 to point 2.
    // 4. Arc anticlockwise from point 2 to point 3. Arc clockwise if this is
    //    the head segment.
    // 5. Line from point 3 to point 0.
    //      2           1
    //       *---------*                   |
    //      /         /                    |
    //      |         |                    |
    //      |         |                    |
    //      \         \                    |
    //       *--------*
    //      3          0
    DCHECK_EQ(4u, ordered_points.size());
    path_.moveTo(ordered_points[0].x(), ordered_points[0].y());
    if (!is_first_segment) {
      path_.arcTo(start_radius, start_radius, 180.0f, gfx::Path::kSmall_ArcSize,
                  gfx::Path::kCW_Direction, ordered_points[1].x(),
                  ordered_points[1].y());
    }

    path_.lineTo(ordered_points[2].x(), ordered_points[2].y());
    path_.arcTo(
        end_radius, end_radius, 180.0f, gfx::Path::kSmall_ArcSize,
        is_last_segment ? gfx::Path::kCW_Direction : gfx::Path::kCCW_Direction,
        ordered_points[3].x(), ordered_points[3].y());
    path_.lineTo(ordered_points[0].x(), ordered_points[0].y());

    // Store data to be used by the next segment.
    path_points_.push_back(ordered_points[2]);
    path_points_.push_back(ordered_points[3]);
  }

  SkPath path() const { return path_; }
  std::vector<gfx::PointF> path_points() const { return path_points_; }

 private:
  SkPath path_;
  std::vector<gfx::PointF> path_points_;

  DISALLOW_COPY_AND_ASSIGN(LaserSegment);
};

// LaserPointerView
LaserPointerView::LaserPointerView(base::TimeDelta life_duration,
                                   base::TimeDelta presentation_delay,
                                   aura::Window* root_window)
    : FastInkView(root_window),
      laser_points_(life_duration),
      predicted_laser_points_(life_duration),
      presentation_delay_(presentation_delay) {}

LaserPointerView::~LaserPointerView() {
}

void LaserPointerView::Stop() {
  UpdateDamageRect(GetBoundingBox());
  laser_points_.Clear();
  predicted_laser_points_.Clear();
  RequestRedraw();
}

void LaserPointerView::AddNewPoint(const gfx::PointF& new_point,
                                   const base::TimeTicks& new_time) {
  TRACE_EVENT1("ui", "LaserPointerView::AddNewPoint", "new_point",
               new_point.ToString());
  TRACE_COUNTER1(
      "ui", "LaserPointerPredictionError",
      predicted_laser_points_.GetNumberOfPoints()
          ? std::round((new_point -
                        predicted_laser_points_.laser_points().front().location)
                           .Length())
          : 0);

  UpdateDamageRect(GetBoundingBox());
  laser_points_.AddPoint(new_point, new_time);

  // Current time is needed to determine presentation time and the number of
  // predicted points to add.
  base::TimeTicks current_time = ui::EventTimeForNow();

  // Create a new set of predicted points based on the last four points added.
  // We add enough predicted points to fill the time between the new point and
  // the expected presentation time. Note that estimated presentation time is
  // based on current time and inefficient rendering of points can result in an
  // actual presentation time that is later.
  predicted_laser_points_.Clear();

  // Normalize all coordinates to screen size.
  gfx::Size screen_size =
      GetWidget()->GetNativeView()->GetBoundsInScreen().size();
  gfx::Vector2dF scale(1.0f / screen_size.width(), 1.0f / screen_size.height());

  // TODO(reveman): Determine interval based on history when event time stamps
  // are accurate. b/36137953
  const float kPredictionIntervalMs = 5.0f;
  const float kMaxPointIntervalMs = 10.0f;
  base::TimeDelta prediction_interval =
      base::TimeDelta::FromMilliseconds(kPredictionIntervalMs);
  base::TimeDelta max_point_interval =
      base::TimeDelta::FromMilliseconds(kMaxPointIntervalMs);
  base::TimeTicks last_point_time = new_time;
  gfx::PointF last_point_location =
      gfx::ScalePoint(new_point, scale.x(), scale.y());

  // Use the last four points for prediction.
  using PositionArray = std::array<gfx::PointF, 4>;
  PositionArray position;
  PositionArray::iterator it = position.begin();
  for (const auto& point : base::Reversed(laser_points_.laser_points())) {
    // Stop adding positions if interval between points is too large to provide
    // an accurate history for prediction.
    if ((last_point_time - point.time) > max_point_interval)
      break;

    last_point_time = point.time;
    last_point_location = gfx::ScalePoint(point.location, scale.x(), scale.y());
    *it++ = last_point_location;

    // Stop when no more positions are needed.
    if (it == position.end())
      break;
  }
  // Pad with last point if needed.
  std::fill(it, position.end(), last_point_location);

  // Note: Currently there's no need to divide by the time delta between
  // points as we assume a constant delta between points that matches the
  // prediction point interval.
  gfx::Vector2dF velocity[3];
  for (size_t i = 0; i < arraysize(velocity); ++i)
    velocity[i] = position[i] - position[i + 1];

  gfx::Vector2dF acceleration[2];
  for (size_t i = 0; i < arraysize(acceleration); ++i)
    acceleration[i] = velocity[i] - velocity[i + 1];

  gfx::Vector2dF jerk = acceleration[0] - acceleration[1];

  // Adjust max prediction time based on speed as prediction data is not great
  // at lower speeds.
  const float kMaxPredictionScaleSpeed = 1e-5;
  double speed = velocity[0].LengthSquared();
  base::TimeTicks max_prediction_time =
      current_time +
      std::min(presentation_delay_ * (speed / kMaxPredictionScaleSpeed),
               presentation_delay_);

  // Add predicted points until we reach the max prediction time.
  gfx::PointF location = position[0];
  for (base::TimeTicks time = new_time + prediction_interval;
       time < max_prediction_time; time += prediction_interval) {
    // Note: Currently there's no need to multiply by the prediction interval
    // as the velocity is calculated based on a time delta between points that
    // is the same as the prediction interval.
    velocity[0] += acceleration[0];
    acceleration[0] += jerk;
    location += velocity[0];

    predicted_laser_points_.AddPoint(
        gfx::ScalePoint(location, screen_size.width(), screen_size.height()),
        time);

    // Always stop at three predicted points as a four point history doesn't
    // provide accurate prediction of more points.
    if (predicted_laser_points_.GetNumberOfPoints() == 3)
      break;
  }

  // Move forward to next presentation time.
  base::TimeTicks next_presentation_time = current_time + presentation_delay_;
  laser_points_.MoveForwardToTime(next_presentation_time);
  predicted_laser_points_.MoveForwardToTime(next_presentation_time);

  UpdateDamageRect(GetBoundingBox());
  RequestRedraw();
}

void LaserPointerView::UpdateTime() {
  UpdateDamageRect(GetBoundingBox());
  // Do not add the point but advance the time if the view is in process of
  // fading away.
  base::TimeTicks next_presentation_time =
      ui::EventTimeForNow() + presentation_delay_;
  laser_points_.MoveForwardToTime(next_presentation_time);
  predicted_laser_points_.MoveForwardToTime(next_presentation_time);
  UpdateDamageRect(GetBoundingBox());
  RequestRedraw();
}

gfx::Rect LaserPointerView::GetBoundingBox() {
  // Expand the bounding box so that it includes the radius of the points on the
  // edges and antialiasing.
  gfx::Rect bounding_box = laser_points_.GetBoundingBox();
  bounding_box.Union(predicted_laser_points_.GetBoundingBox());
  const int kOutsetForAntialiasing = 1;
  int outset = kPointInitialRadius + kOutsetForAntialiasing;
  bounding_box.Inset(-outset, -outset);
  return bounding_box;
}

void LaserPointerView::OnRedraw(gfx::Canvas& canvas,
                                const gfx::Vector2d& offset) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  int num_points = laser_points_.GetNumberOfPoints() +
                   predicted_laser_points_.GetNumberOfPoints();
  if (!num_points)
    return;

  LaserPointerPoints::LaserPoint previous_point = laser_points_.GetOldest();
  previous_point.location -= offset;
  LaserPointerPoints::LaserPoint current_point;
  std::vector<gfx::PointF> previous_segment_points;
  float previous_radius;
  int current_opacity;

  for (int i = 0; i < num_points; ++i) {
    if (i < laser_points_.GetNumberOfPoints()) {
      current_point = laser_points_.laser_points()[i];
    } else {
      current_point =
          predicted_laser_points_
              .laser_points()[i - laser_points_.GetNumberOfPoints()];
    }
    current_point.location -= offset;

    // Set the radius and opacity based on the distance.
    float current_radius = LinearInterpolate(
        kPointInitialRadius, kPointFinalRadius, current_point.age);
    current_opacity = static_cast<int>(LinearInterpolate(
        kPointInitialOpacity, kPointFinalOpacity, current_point.age));

    // If we draw laser_points_ that are within a stroke width of each other,
    // the result will be very jagged, unless we are on the last point, then
    // we draw regardless.
    float distance_threshold = current_radius * 2.0f;
    if (DistanceBetweenPoints(previous_point.location,
                              current_point.location) <= distance_threshold &&
        i != num_points - 1) {
      continue;
    }

    LaserSegment current_segment(
        previous_segment_points, gfx::PointF(previous_point.location),
        gfx::PointF(current_point.location), previous_radius, current_radius,
        i == num_points - 1);

    SkPath path = current_segment.path();
    if (i < laser_points_.GetNumberOfPoints())
      flags.setColor(SkColorSetA(kPointColor, current_opacity));
    else
      flags.setColor(SkColorSetA(kPredictionPointColor, current_opacity));
    canvas.DrawPath(path, flags);

    previous_segment_points = current_segment.path_points();
    previous_radius = current_radius;
    previous_point = current_point;
  }

  // Draw the last point as a circle.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas.DrawCircle(current_point.location, kPointInitialRadius, flags);
}

}  // namespace ash
