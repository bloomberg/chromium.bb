// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
#define ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_

#include "ash/display/display_configuration_controller.h"
#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/display/display.h"

namespace ash {
class ScreenOrientationController;

class ScreenOrientationControllerTestApi {
 public:
  explicit ScreenOrientationControllerTestApi(
      ScreenOrientationController* controller);

  void SetDisplayRotation(
      display::Display::Rotation rotation,
      display::Display::RotationSource source,
      DisplayConfigurationController::RotationAnimation mode =
          DisplayConfigurationController::ANIMATION_ASYNC);

  void SetRotationLocked(bool rotation_locked);

  blink::WebScreenOrientationLockType UserLockedOrientation() const;

  blink::WebScreenOrientationLockType GetCurrentOrientation() const;

 private:
  ScreenOrientationController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationControllerTestApi);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
