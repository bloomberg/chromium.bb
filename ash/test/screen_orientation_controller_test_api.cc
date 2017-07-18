// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/screen_orientation_controller_test_api.h"

#include "ash/display/screen_orientation_controller_chromeos.h"

namespace ash {
namespace test {

ScreenOrientationControllerTestApi::ScreenOrientationControllerTestApi(
    ScreenOrientationController* controller)
    : controller_(controller) {}

void ScreenOrientationControllerTestApi::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source) {
  controller_->SetDisplayRotation(rotation, source);
}

void ScreenOrientationControllerTestApi::SetRotationLocked(bool locked) {
  controller_->SetRotationLockedInternal(locked);
}

blink::WebScreenOrientationLockType
ScreenOrientationControllerTestApi::UserLockedOrientation() const {
  return controller_->user_locked_orientation_;
}

blink::WebScreenOrientationLockType
ScreenOrientationControllerTestApi::GetCurrentOrientation() const {
  return controller_->GetCurrentOrientationForTest();
}

}  // namespace test
}  // namespace ash
