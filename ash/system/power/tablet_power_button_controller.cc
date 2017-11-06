// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// Amount of time the power button must be held to start the pre-shutdown
// animation when in tablet mode. This differs depending on whether the screen
// is on or off when the power button is initially pressed.
constexpr base::TimeDelta kShutdownWhenScreenOnTimeout =
    base::TimeDelta::FromMilliseconds(500);
// TODO(derat): This is currently set to a high value to work around delays in
// powerd's reports of button-up events when the preceding button-down event
// turns the display on. Set it to a lower value once powerd no longer blocks on
// asking Chrome to turn the display on: http://crbug.com/685734
constexpr base::TimeDelta kShutdownWhenScreenOffTimeout =
    base::TimeDelta::FromMilliseconds(2000);

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
constexpr base::TimeDelta kIgnorePowerButtonAfterResumeDelay =
    base::TimeDelta::FromSeconds(2);

}  // namespace

constexpr base::TimeDelta TabletPowerButtonController::kScreenStateChangeDelay;

constexpr base::TimeDelta
    TabletPowerButtonController::kIgnoreRepeatedButtonUpDelay;

TabletPowerButtonController::TabletPowerButtonController(
    PowerButtonDisplayController* display_controller,
    base::TickClock* tick_clock)
    : lock_state_controller_(Shell::Get()->lock_state_controller()),
      display_controller_(display_controller),
      tick_clock_(tick_clock) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

TabletPowerButtonController::~TabletPowerButtonController() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void TabletPowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  if (down) {
    force_off_on_button_up_ = true;

    // When the system resumes in response to the power button being pressed,
    // Chrome receives powerd's SuspendDone signal and notification that the
    // backlight has been turned back on before seeing the power button events
    // that woke the system. Avoid forcing off display just after resuming to
    // ensure that we don't turn the display off in response to the events.
    if (timestamp - last_resume_time_ <= kIgnorePowerButtonAfterResumeDelay)
      force_off_on_button_up_ = false;

    // The actual display may remain off for a short period after powerd asks
    // Chrome to turn it on. If the user presses the power button again during
    // this time, they probably intend to turn the display on. Avoid forcing off
    // in this case.
    if (timestamp - display_controller_->screen_state_last_changed() <=
        kScreenStateChangeDelay) {
      force_off_on_button_up_ = false;
    }

    screen_off_when_power_button_down_ =
        display_controller_->screen_state() !=
        PowerButtonDisplayController::ScreenState::ON;
    display_controller_->SetDisplayForcedOff(false);
    StartShutdownTimer();
  } else {
    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = timestamp;

    // When power button is released, cancel shutdown animation whenever it is
    // still cancellable.
    if (lock_state_controller_->CanCancelShutdownAnimation())
      lock_state_controller_->CancelShutdownAnimation();

    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <= kIgnoreRepeatedButtonUpDelay) {
      shutdown_timer_.Stop();
      return;
    }

    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_ && force_off_on_button_up_) {
        display_controller_->SetDisplayForcedOff(true);
        LockScreenIfRequired();
      }
    }
  }
}

void TabletPowerButtonController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
}

void TabletPowerButtonController::OnTabletModeStarted() {
  shutdown_timer_.Stop();
  if (lock_state_controller_->CanCancelShutdownAnimation())
    lock_state_controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnTabletModeEnded() {
  shutdown_timer_.Stop();
  if (lock_state_controller_->CanCancelShutdownAnimation())
    lock_state_controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::CancelTabletPowerButton() {
  if (lock_state_controller_->CanCancelShutdownAnimation())
    lock_state_controller_->CancelShutdownAnimation();
  force_off_on_button_up_ = false;
  shutdown_timer_.Stop();
}

void TabletPowerButtonController::StartShutdownTimer() {
  base::TimeDelta timeout = screen_off_when_power_button_down_
                                ? kShutdownWhenScreenOffTimeout
                                : kShutdownWhenScreenOnTimeout;
  shutdown_timer_.Start(FROM_HERE, timeout, this,
                        &TabletPowerButtonController::OnShutdownTimeout);
}

void TabletPowerButtonController::OnShutdownTimeout() {
  lock_state_controller_->StartShutdownAnimation(ShutdownReason::POWER_BUTTON);
}

void TabletPowerButtonController::LockScreenIfRequired() {
  SessionController* session_controller = Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !lock_state_controller_->LockRequested()) {
    lock_state_controller_->LockWithoutAnimation();
  }
}

}  // namespace ash
