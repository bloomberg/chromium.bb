// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_view.h"

#include <memory>

#include "ash/laser/laser_pointer_points.h"
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
const double kPointInitialRadius = 5;
const double kPointFinalRadius = 0.25;
const int kPointInitialOpacity = 200;
const int kPointFinalOpacity = 0;
const SkColor kPointColor = SkColorSetRGB(255, 0, 0);

float DistanceBetweenPoints(const gfx::Point& point1,
                            const gfx::Point& point2) {
  return (point1 - point2).Length();
}

double LinearInterpolate(double initial_value,
                         double final_value,
                         double progress) {
  return initial_value + (final_value - initial_value) * progress;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

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
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setAntiAlias(true);
  paint.setStrokeJoin(SkPaint::kBevel_Join);

  // Compute the offset of the current widget.
  gfx::Point widget_offset =
      widget_->GetNativeView()->GetBoundsInRootWindow().origin();

  gfx::Point previous_point(
      (laser_points_.GetOldest().location - widget_offset).x(),
      (laser_points_.GetOldest().location - widget_offset).y());
  gfx::Point current_point;

  int num_points_ = laser_points_.GetNumberOfPoints();
  int point_count = 0;
  int current_opacity = 0;
  for (const LaserPointerPoints::LaserPoint& point :
       laser_points_.laser_points()) {
    // Set the radius and opacity based on the distance.
    double current_radius =
        LinearInterpolate(kPointInitialRadius, kPointFinalRadius, point.age);
    current_opacity = int{LinearInterpolate(
        double{kPointInitialOpacity}, double{kPointFinalOpacity}, point.age)};

    gfx::Vector2d center = point.location - widget_offset;
    current_point = gfx::Point(center.x(), center.y());

    // If we draw laser_points_ that are within a stroke width of each other,
    // the result will be very jagged, unless we are on the last point, then we
    // draw regardless.
    point_count++;
    float distance_threshold = float{current_radius * 2};
    if (DistanceBetweenPoints(previous_point, current_point) <=
            distance_threshold &&
        point_count != num_points_) {
      continue;
    }

    paint.setColor(SkColorSetA(kPointColor, current_opacity));
    paint.setStrokeWidth(current_radius * 2);
    canvas->DrawLine(previous_point, current_point, paint);
    previous_point = current_point;
  }
  // Draw the last point as a circle.
  paint.setColor(SkColorSetA(kPointColor, current_opacity));
  paint.setStyle(SkPaint::kFill_Style);
  canvas->DrawCircle(current_point, kPointInitialRadius, paint);
}
}  // namespace ash
