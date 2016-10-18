// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_points_test_api.h"

namespace ash {

LaserPointerPointsTestApi::LaserPointerPointsTestApi(
    LaserPointerPoints* instance)
    : instance_(instance) {}

LaserPointerPointsTestApi::~LaserPointerPointsTestApi() {}

int LaserPointerPointsTestApi::GetNumberOfPoints() const {
  return instance_->GetNumberOfPoints();
}

void LaserPointerPointsTestApi::MoveForwardInTime(
    const base::TimeDelta& delta) {
  base::Time new_time = instance_->collection_latest_time_ + delta;
  instance_->MoveForwardToTime(new_time);

  LaserPointerPoints::LaserPoint new_point;
  instance_->points_.push_back(new_point);
}

LaserPointerPoints::LaserPoint LaserPointerPointsTestApi::GetPointAtIndex(
    int index) {
  DCHECK(index >= 0 && index < GetNumberOfPoints());
  return instance_->points_[index];
}

}  // namespace ash
