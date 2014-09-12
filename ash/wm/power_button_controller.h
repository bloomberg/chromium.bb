// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_POWER_BUTTON_CONTROLLER_H_
#define ASH_WM_POWER_BUTTON_CONTROLLER_H_

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_export.h"
#include "ash/wm/session_state_animator.h"
#include "base/basictypes.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif

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

class LockStateController;

// Handles power & lock button events which may result in the locking or
// shutting down of the system as well as taking screen shots while in maximize
// mode.
class ASH_EXPORT PowerButtonController : ui::EventHandler
// TODO(derat): Remove these ifdefs after DisplayConfigurator becomes
// cross-platform.
#if defined(OS_CHROMEOS)
      , public ui::DisplayConfigurator::Observer
#endif
      {
 public:
  explicit PowerButtonController(LockStateController* controller);
  virtual ~PowerButtonController();

  void set_has_legacy_power_button_for_test(bool legacy) {
    has_legacy_power_button_ = legacy;
  }

  void set_enable_quick_lock_for_test(bool enable_quick_lock) {
    enable_quick_lock_ = enable_quick_lock;
  }

  // Called when the current screen brightness changes.
  void OnScreenBrightnessChanged(double percent);

  // Called when the power or lock buttons are pressed or released.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overriden from ui::DisplayConfigurator::Observer:
  virtual void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) OVERRIDE;
#endif

 private:
  friend class test::PowerButtonControllerTest;

  // Are the power or lock buttons currently held?
  bool power_button_down_;
  bool lock_button_down_;

  // True when the volume down button is being held down.
  bool volume_down_pressed_;

  // Has the screen brightness been reduced to 0%?
  bool brightness_is_zero_;

  // True if an internal display is off while an external display is on (e.g.
  // for Chrome OS's docked mode, where a Chromebook's lid is closed while an
  // external display is connected).
  bool internal_display_off_and_external_display_on_;

  // Was a command-line switch set telling us that we're running on hardware
  // that misreports power button releases?
  bool has_legacy_power_button_;

  // Enables quick, non-cancellable locking of the screen when in maximize mode.
  bool enable_quick_lock_;

  LockStateController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace ash

#endif  // ASH_WM_POWER_BUTTON_CONTROLLER_H_
