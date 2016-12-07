// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_view.h"

#include <memory>

#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_segment_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
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

float DistanceBetweenPoints(const gfx::Point& point1,
                            const gfx::Point& point2) {
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
                                   aura::Window* root_window)
    : laser_points_(life_duration) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "LaserOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);

  widget_->Init(params);
  widget_->Show();
  widget_->SetContentsView(this);
  set_owned_by_client();
}

LaserPointerView::~LaserPointerView() {}

void LaserPointerView::Stop() {
  laser_points_.Clear();
  SchedulePaint();
}

void LaserPointerView::AddNewPoint(const gfx::Point& new_point) {
  laser_points_.AddPoint(new_point);
  OnPointsUpdated();
}

void LaserPointerView::UpdateTime() {
  // Do not add the point but advance the time if the view is in process of
  // fading away.
  laser_points_.MoveForwardToTime(base::Time::Now());
  OnPointsUpdated();
}

void LaserPointerView::OnPointsUpdated() {
  // The bounding box should be relative to the screen.
  gfx::Point screen_offset =
      widget_->GetNativeView()->GetRootWindow()->GetBoundsInScreen().origin();

  // Expand the bounding box so that it includes the radius of the points on the
  // edges.
  gfx::Rect bounding_box;
  bounding_box = laser_points_.GetBoundingBox();
  bounding_box.Offset(-kPointInitialRadius, -kPointInitialRadius);
  bounding_box.Offset(screen_offset.x(), screen_offset.y());
  bounding_box.set_width(bounding_box.width() + (kPointInitialRadius * 2));
  bounding_box.set_height(bounding_box.height() + (kPointInitialRadius * 2));
  widget_->SetBounds(bounding_box);
  SchedulePaint();
}

void LaserPointerView::OnPaint(gfx::Canvas* canvas) {
  if (laser_points_.IsEmpty())
    return;

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  // Compute the offset of the current widget.
  gfx::Vector2d widget_offset(
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().x(),
      widget_->GetNativeView()->GetBoundsInRootWindow().origin().y());

  int num_points = laser_points_.GetNumberOfPoints();
  DCHECK(num_points > 0);
  LaserPointerPoints::LaserPoint previous_point = laser_points_.GetOldest();
  previous_point.location -= widget_offset;
  LaserPointerPoints::LaserPoint current_point;
  std::vector<gfx::PointF> previous_segment_points;
  float previous_radius;
  int current_opacity;

  for (int i = 0; i < num_points; ++i) {
    current_point = laser_points_.laser_points()[i];
    current_point.location -= widget_offset;

    // Set the radius and opacity based on the distance.
    float current_radius = LinearInterpolate(
        kPointInitialRadius, kPointFinalRadius, current_point.age);
    current_opacity = int{LinearInterpolate(
        kPointInitialOpacity, kPointFinalOpacity, current_point.age)};

    // If we draw laser_points_ that are within a stroke width of each other,
    // the result will be very jagged, unless we are on the last point, then we
    // draw regardless.
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
    paint.setColor(SkColorSetA(kPointColor, current_opacity));
    canvas->DrawPath(path, paint);

    previous_segment_points = current_segment.path_points();
    previous_radius = current_radius;
    previous_point = current_point;
  }
  // Draw the last point as a circle.
  paint.setColor(SkColorSetA(kPointColor, current_opacity));
  paint.setStyle(SkPaint::kFill_Style);
  canvas->DrawCircle(current_point.location, kPointInitialRadius, paint);
}
}  // namespace ash
