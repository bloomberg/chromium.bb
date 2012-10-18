// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_POWER_BUTTON_CONTROLLER_H_
#define ASH_WM_POWER_BUTTON_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/session_state_animator.h"
#include "base/basictypes.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Layer;
}

namespace ash {

namespace test {
class PowerButtonControllerTest;
}

class SessionStateController;

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
class ASH_EXPORT PowerButtonController {
 public:

  explicit PowerButtonController(SessionStateController* controller);
  virtual ~PowerButtonController();

  void set_has_legacy_power_button_for_test(bool legacy) {
    has_legacy_power_button_ = legacy;
  }

  // Called when the current screen brightness changes.
  void OnScreenBrightnessChanged(double percent);

  // Called when the power or lock buttons are pressed or released.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

 private:
  friend class test::PowerButtonControllerTest;

  // Are the power or lock buttons currently held?
  bool power_button_down_;
  bool lock_button_down_;

  // Is the screen currently turned off?
  bool screen_is_off_;

  // Was a command-line switch set telling us that we're running on hardware
  // that misreports power button releases?
  bool has_legacy_power_button_;

  SessionStateController* controller_; // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace ash

#endif  // ASH_WM_POWER_BUTTON_CONTROLLER_H_
