// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_

#include <memory>
#include <utility>

#include "ash/ash_export.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace base {
class TickClock;
}  // namespace base

namespace views {
class Widget;
}  // namespace views

namespace ash {
class LockStateController;
class PowerButtonDisplayController;

// Handles power button events on convertible/tablet device. This class is
// instantiated and used in PowerButtonController.
class ASH_EXPORT TabletPowerButtonController
    : public BacklightsForcedOffSetter::Observer,
      public chromeos::PowerManagerClient::Observer,
      public TabletModeObserver {
 public:
  // Amount of time since last screen state change that power button event needs
  // to be ignored.
  static constexpr base::TimeDelta kScreenStateChangeDelay =
      base::TimeDelta::FromMilliseconds(500);

  // Ignore button-up events occurring within this many milliseconds of the
  // previous button-up event. This prevents us from falling behind if the power
  // button is pressed repeatedly.
  static constexpr base::TimeDelta kIgnoreRepeatedButtonUpDelay =
      base::TimeDelta::FromMilliseconds(500);

  // Amount of time since last SuspendDone() that power button event needs to be
  // ignored.
  static constexpr base::TimeDelta kIgnorePowerButtonAfterResumeDelay =
      base::TimeDelta::FromSeconds(2);

  TabletPowerButtonController(
      PowerButtonDisplayController* display_controller,
      BacklightsForcedOffSetter* backlights_forced_off_setter,
      bool show_power_button_menu,
      base::TickClock* tick_clock);
  ~TabletPowerButtonController() override;

  // Handles a power button event.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // True if the menu is opened.
  bool IsMenuOpened() const;

  // Dismisses the menu.
  void DismissMenu();

  // Cancel the ongoing tablet power button behavior.
  void CancelTabletPowerButton();

  // BacklightsForcedOffSetter::Observer:
  void OnBacklightsForcedOffChanged(bool forced_off) override;
  void OnScreenStateChanged(
      BacklightsForcedOffSetter::ScreenState screen_state) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  friend class TabletPowerButtonControllerTestApi;

  // Starts |shutdown_timer_| when the power button is pressed while in
  // tablet mode.
  void StartShutdownTimer();

  // Called by |shutdown_timer_| to start the pre-shutdown animation.
  void OnShutdownTimeout();

  // Locks the screen if the "Show lock screen when waking from sleep" pref is
  // set and locking is possible.
  void LockScreenIfRequired();

  // Called by |power_button_menu_timer_| to start showing the power button
  // menu.
  void OnPowerButtonMenuTimeout();

  // True if the screen was off when the power button was pressed.
  bool screen_off_when_power_button_down_ = false;

  // Saves the most recent timestamp that powerd is resuming from suspend,
  // updated in SuspendDone().
  base::TimeTicks last_resume_time_;

  // Saves the most recent timestamp that power button is released.
  base::TimeTicks last_button_up_time_;

  // True if power button released should force off display.
  bool force_off_on_button_up_ = true;

  // Started when the tablet power button is pressed and stopped when it's
  // released. Runs OnShutdownTimeout() to start shutdown.
  base::OneShotTimer shutdown_timer_;

  // Used when |show_power_button_menu_| is true. Started when the power button
  // is pressed and stopped when it's released. Runs OnPowerButtonMenuTimeout()
  // to show the power button menu.
  base::OneShotTimer power_button_menu_timer_;

  // The fullscreen widget of power button menu.
  std::unique_ptr<views::Widget> menu_widget_;

  LockStateController* lock_state_controller_;  // Not owned.

  // Used to interact with the display.
  PowerButtonDisplayController* display_controller_;  // Not owned.

  ScopedObserver<BacklightsForcedOffSetter, BacklightsForcedOffSetter::Observer>
      backlights_forced_off_observer_;

  // If true, start the |power_button_menu_timer_| to show the menu when the
  // power button is long pressed.
  bool show_power_button_menu_ = false;

  // Time source for performed action times.
  base::TickClock* tick_clock_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
