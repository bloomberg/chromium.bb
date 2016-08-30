// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode_test_api.h"

#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_points.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_view.h"

namespace ash {

LaserPointerModeTestApi::LaserPointerModeTestApi(
    std::unique_ptr<LaserPointerMode> instance)
    : instance_(std::move(instance)) {}

LaserPointerModeTestApi::~LaserPointerModeTestApi() {}

void LaserPointerModeTestApi::OnEnable() {
  instance_->OnEnable();
}

void LaserPointerModeTestApi::OnDisable() {
  instance_->OnDisable();
}

const LaserPointerPoints& LaserPointerModeTestApi::laser_points() {
  return instance_->laser_pointer_view_->laser_points_;
}

const gfx::Point& LaserPointerModeTestApi::current_mouse_location() {
  return instance_->current_mouse_location_;
}

}  // namespace ash
