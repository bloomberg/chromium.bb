// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_points_test_api.h"

#include "ash/laser/laser_pointer_points.h"

namespace ash {

LaserPointerPointsTestApi::LaserPointerPointsTestApi(
    LaserPointerPoints* instance)
    : new_point_time_(base::Time::Now()), instance_(instance) {}

LaserPointerPointsTestApi::~LaserPointerPointsTestApi() {}

int LaserPointerPointsTestApi::GetNumberOfPoints() const {
  return instance_->GetNumberOfPoints();
}

void LaserPointerPointsTestApi::MoveForwardInTime(
    const base::TimeDelta& delta) {
  for (LaserPointerPoints::LaserPoint& point : instance_->points_)
    point.creation_time -= delta;

  LaserPointerPoints::LaserPoint new_point;
  new_point.creation_time = new_point_time_;
  instance_->points_.push_back(new_point);
  instance_->ClearOldPoints();
}
}  // namespace ash
