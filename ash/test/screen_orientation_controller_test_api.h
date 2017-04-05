// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
#define ASH_TEST_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/display/display.h"

namespace ash {
class ScreenOrientationController;

namespace test {

class ScreenOrientationControllerTestApi {
 public:
  explicit ScreenOrientationControllerTestApi(
      ScreenOrientationController* controller);

  void SetDisplayRotation(display::Display::Rotation rotation,
                          display::Display::RotationSource source);

  void SetRotationLocked(bool rotation_locked);

  blink::WebScreenOrientationLockType UserLockedOrientation() const;

  blink::WebScreenOrientationLockType GetCurrentOrientation() const;

 private:
  ScreenOrientationController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationControllerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SCREEN_ORIENTATION_CONTROLLER_TEST_API_H_
