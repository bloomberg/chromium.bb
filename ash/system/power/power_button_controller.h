// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/events/event_handler.h"

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace ash {

class LockStateController;
class PowerButtonDisplayController;
class TabletPowerButtonController;

// Handles power & lock button events which may result in the locking or
// shutting down of the system as well as taking screen shots while in maximize
// mode.
class ASH_EXPORT PowerButtonController
    : public ui::EventHandler,
      public display::DisplayConfigurator::Observer,
      public chromeos::PowerManagerClient::Observer,
      public chromeos::AccelerometerReader::Observer {
 public:
  enum class ButtonType {
    // Indicates normal power button type.
    NORMAL,

    // Indicates legacy power button type. It could be set by command-line
    // switch telling us that we're running on hardware that misreports power
    // button releases.
    LEGACY,
  };

  PowerButtonController();
  ~PowerButtonController() override;

  // Called when the power or lock buttons are pressed or released.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Overridden from display::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;

  // Overridden from chromeos::PowerManagerClient::Observer:
  void BrightnessChanged(int level, bool user_initiated) override;
  void PowerButtonEventReceived(bool down,
                                const base::TimeTicks& timestamp) override;

  // Overridden from chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  // If |display_off_timer_| is running, stops it, runs its task, and returns
  // true. Otherwise, returns false.
  bool TriggerDisplayOffTimerForTesting() WARN_UNUSED_RESULT;

  TabletPowerButtonController* tablet_power_button_controller_for_test() {
    return tablet_controller_.get();
  }

  void set_power_button_type_for_test(ButtonType button_type) {
    button_type_ = button_type;
  }

 private:
  // Updates |button_type_| and |force_clamshell_power_button_| based on the
  // current command line.
  void ProcessCommandLine();

  // Called by |display_off_timer_| to force backlights off shortly after the
  // screen is locked. Only used when |force_clamshell_power_button_| is true.
  void ForceDisplayOffAfterLock();

  // Are the power or lock buttons currently held?
  bool power_button_down_ = false;
  bool lock_button_down_ = false;

  // True when the volume down button is being held down.
  bool volume_down_pressed_ = false;

  // Volume to be restored after a screenshot is taken by pressing the power
  // button while holding VKEY_VOLUME_DOWN.
  int volume_percent_before_screenshot_ = 0;

  // Has the screen brightness been reduced to 0%?
  bool brightness_is_zero_ = false;

  // True if an internal display is off while an external display is on (e.g.
  // for Chrome OS's docked mode, where a Chromebook's lid is closed while an
  // external display is connected).
  bool internal_display_off_and_external_display_on_ = false;

  // Saves the button type for this power button.
  ButtonType button_type_ = ButtonType::NORMAL;

  // Was a command-line switch set telling us to use non-tablet-style power
  // button behavior even if we're running on a convertible device?
  bool force_clamshell_power_button_ = false;

  // True if the lock animation was started for the last power button down
  // event.
  bool started_lock_animation_for_power_button_down_ = false;

  LockStateController* lock_state_controller_;  // Not owned.

  // Time source for performed action times.
  std::unique_ptr<base::TickClock> tick_clock_;

  // Used to interact with the display.
  std::unique_ptr<PowerButtonDisplayController> display_controller_;

  // Handles events for convertible/tablet devices.
  std::unique_ptr<TabletPowerButtonController> tablet_controller_;

  // Used to run ForceDisplayOffAfterLock() shortly after the screen is locked.
  // Only started when |force_clamshell_power_button_| is true.
  base::OneShotTimer display_off_timer_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_
