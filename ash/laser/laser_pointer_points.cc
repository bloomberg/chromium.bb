// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_points.h"

#include <algorithm>
#include <limits>

#include "ui/gfx/geometry/rect_conversions.h"

namespace ash {

LaserPointerPoints::LaserPointerPoints(base::TimeDelta life_duration)
    : life_duration_(life_duration) {}

LaserPointerPoints::~LaserPointerPoints() {}

void LaserPointerPoints::AddPoint(const gfx::PointF& point,
                                  const base::TimeTicks& time) {
  // Move forward in time if needed.
  if (time > collection_latest_time_)
    MoveForwardToTime(time);

  LaserPoint new_point;
  new_point.location = point;
  new_point.time = time;
  new_point.age = std::min((collection_latest_time_ - time).InMillisecondsF() /
                               life_duration_.InMillisecondsF(),
                           1.0);
  points_.push_back(new_point);
}

void LaserPointerPoints::MoveForwardToTime(const base::TimeTicks& latest_time) {
  DCHECK_GE(latest_time, collection_latest_time_);

  if (!points_.empty()) {
    DCHECK(!collection_latest_time_.is_null());

    // Increase the age of points based on how much time has elapsed.
    base::TimeDelta delta = latest_time - collection_latest_time_;
    double lifespan_change =
        delta.InMillisecondsF() / life_duration_.InMillisecondsF();
    for (LaserPoint& point : points_)
      point.age += lifespan_change;

    // Remove points that are too old (points age older than 1.0).
    auto first_alive_point =
        std::find_if(points_.begin(), points_.end(),
                     [](const LaserPoint& p) { return p.age < 1.0; });
    points_.erase(points_.begin(), first_alive_point);
  }
  collection_latest_time_ = latest_time;
}

void LaserPointerPoints::Clear() {
  points_.clear();
}

gfx::Rect LaserPointerPoints::GetBoundingBox() {
  if (IsEmpty())
    return gfx::Rect();

  gfx::PointF min_point = GetOldest().location;
  gfx::PointF max_point = GetOldest().location;
  for (const LaserPoint& point : points_) {
    min_point.SetToMin(point.location);
    max_point.SetToMax(point.location);
  }
  return gfx::ToEnclosingRect(gfx::BoundingRect(min_point, max_point));
}

LaserPointerPoints::LaserPoint LaserPointerPoints::GetOldest() const {
  DCHECK(!IsEmpty());
  return points_.front();
}

LaserPointerPoints::LaserPoint LaserPointerPoints::GetNewest() const {
  DCHECK(!IsEmpty());
  return points_.back();
}

bool LaserPointerPoints::IsEmpty() const {
  return points_.empty();
}

int LaserPointerPoints::GetNumberOfPoints() const {
  return points_.size();
}

const std::deque<LaserPointerPoints::LaserPoint>&
LaserPointerPoints::laser_points() const {
  return points_;
}
}  // namespace ash
