// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller_test_api.h"

#include "ash/laser/laser_pointer_controller.h"
#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_pointer_view.h"

namespace ash {

LaserPointerControllerTestApi::LaserPointerControllerTestApi(
    LaserPointerController* instance)
    : instance_(instance) {}

LaserPointerControllerTestApi::~LaserPointerControllerTestApi() {}

void LaserPointerControllerTestApi::SetEnabled(bool enabled) {
  instance_->SetEnabled(enabled);
}

bool LaserPointerControllerTestApi::IsShowingLaserPointer() {
  return instance_->laser_pointer_view_ != nullptr;
}

bool LaserPointerControllerTestApi::IsFadingAway() {
  return IsShowingLaserPointer() && instance_->is_fading_away_;
}

void LaserPointerControllerTestApi::SetIsFadingAway(bool fading_away) {
  instance_->is_fading_away_ = fading_away;
}

const LaserPointerPoints& LaserPointerControllerTestApi::laser_points() {
  return instance_->laser_pointer_view_->laser_points_;
}

LaserPointerView* LaserPointerControllerTestApi::laser_pointer_view() {
  return instance_->laser_pointer_view_.get();
}

}  // namespace ash
