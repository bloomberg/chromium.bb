// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// Amount of time the power button must be held to start the pre-shutdown
// animation when in tablet mode. This differs depending on whether the screen
// is on or off when the power button is initially pressed.
constexpr int kShutdownWhenScreenOnTimeoutMs = 500;
// TODO(derat): This is currently set to a high value to work around delays in
// powerd's reports of button-up events when the preceding button-down event
// turns the display on. Set it to a lower value once powerd no longer blocks on
// asking Chrome to turn the display on: http://crbug.com/685734
constexpr int kShutdownWhenScreenOffTimeoutMs = 2000;

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
constexpr int kIgnorePowerButtonAfterResumeMs = 2000;

// Ignore button-up events occurring within this many milliseconds of the
// previous button-up event. This prevents us from falling behind if the power
// button is pressed repeatedly.
constexpr int kIgnoreRepeatedButtonUpMs = 500;

// Returns true if device is a convertible/tablet device, otherwise false.
bool IsTabletModeSupported() {
  MaximizeModeController* maximize_mode_controller =
      Shell::Get()->maximize_mode_controller();
  return maximize_mode_controller &&
         maximize_mode_controller->CanEnterMaximizeMode();
}

// Returns true if device is currently in tablet/maximize mode, otherwise false.
bool IsTabletModeActive() {
  MaximizeModeController* maximize_mode_controller =
      Shell::Get()->maximize_mode_controller();
  return maximize_mode_controller &&
         maximize_mode_controller->IsMaximizeModeWindowManagerEnabled();
}

}  // namespace

TabletPowerButtonController::TestApi::TestApi(
    TabletPowerButtonController* controller)
    : controller_(controller) {}

TabletPowerButtonController::TestApi::~TestApi() {}

bool TabletPowerButtonController::TestApi::ShutdownTimerIsRunning() const {
  return controller_->shutdown_timer_.IsRunning();
}

void TabletPowerButtonController::TestApi::TriggerShutdownTimeout() {
  DCHECK(ShutdownTimerIsRunning());
  controller_->OnShutdownTimeout();
  controller_->shutdown_timer_.Stop();
}

TabletPowerButtonController::TabletPowerButtonController(
    LockStateController* controller)
    : tick_clock_(new base::DefaultTickClock()),
      force_off_on_button_up_(true),
      controller_(controller),
      weak_ptr_factory_(this) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::Get()->AddShellObserver(this);
  // TODO(mash): Provide a way for this class to observe stylus events:
  // http://crbug.com/682460
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->AddObserver(this);
  Shell::Get()->PrependPreTargetHandler(this);

  GetInitialBacklightsForcedOff();
}

TabletPowerButtonController::~TabletPowerButtonController() {
  Shell::Get()->RemovePreTargetHandler(this);
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

bool TabletPowerButtonController::ShouldHandlePowerButtonEvents() const {
  return IsTabletModeSupported();
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
    if (timestamp - last_resume_time_ <=
        base::TimeDelta::FromMilliseconds(kIgnorePowerButtonAfterResumeMs)) {
      force_off_on_button_up_ = false;
    }
    screen_off_when_power_button_down_ = brightness_level_is_zero_;
    SetDisplayForcedOff(false);
    StartShutdownTimer();
  } else {
    // When power button is released, cancel shutdown animation whenever it is
    // still cancellable.
    if (controller_->CanCancelShutdownAnimation())
      controller_->CancelShutdownAnimation();

    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = tick_clock_->NowTicks();
    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <=
        base::TimeDelta::FromMilliseconds(kIgnoreRepeatedButtonUpMs)) {
      shutdown_timer_.Stop();
      return;
    }

    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_ && force_off_on_button_up_) {
        SetDisplayForcedOff(true);
        LockScreenIfRequired();
      }
    }
  }
}

void TabletPowerButtonController::PowerManagerRestarted() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(backlights_forced_off_);
}

void TabletPowerButtonController::BrightnessChanged(int level,
                                                    bool user_initiated) {
  brightness_level_is_zero_ = level == 0;
}

void TabletPowerButtonController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
}

void TabletPowerButtonController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& timestamp) {
  SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnMaximizeModeStarted() {
  shutdown_timer_.Stop();
  if (controller_->CanCancelShutdownAnimation())
    controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnMaximizeModeEnded() {
  shutdown_timer_.Stop();
  if (controller_->CanCancelShutdownAnimation())
    controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore key events generated by the power button since power button activity
  // is already handled by OnPowerButtonEvent().
  if (event->key_code() == ui::VKEY_POWER)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnStylusStateChanged(ui::StylusState state) {
  if (IsTabletModeSupported() && state == ui::StylusState::REMOVED &&
      backlights_forced_off_) {
    SetDisplayForcedOff(false);
  }
}

void TabletPowerButtonController::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(tick_clock);
  tick_clock_ = std::move(tick_clock);
}

void TabletPowerButtonController::SetDisplayForcedOff(bool forced_off) {
  if (backlights_forced_off_ == forced_off)
    return;

  // Set the display and keyboard backlights (if present) to |forced_off|.
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(forced_off);
  backlights_forced_off_ = forced_off;
  UpdateTouchscreenStatus();

  // Send an a11y alert.
  Shell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
      forced_off ? A11Y_ALERT_SCREEN_OFF : A11Y_ALERT_SCREEN_ON);
}

void TabletPowerButtonController::GetInitialBacklightsForcedOff() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetBacklightsForcedOff(base::Bind(
          &TabletPowerButtonController::OnGotInitialBacklightsForcedOff,
          weak_ptr_factory_.GetWeakPtr()));
}

void TabletPowerButtonController::OnGotInitialBacklightsForcedOff(
    bool is_forced_off) {
  backlights_forced_off_ = is_forced_off;
  UpdateTouchscreenStatus();
}

void TabletPowerButtonController::UpdateTouchscreenStatus() {
  ShellDelegate* delegate = Shell::Get()->shell_delegate();
  delegate->SetTouchscreenEnabledInPrefs(!backlights_forced_off_,
                                         true /* use_local_state */);
  delegate->UpdateTouchscreenStatusFromPrefs();
}

void TabletPowerButtonController::StartShutdownTimer() {
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      screen_off_when_power_button_down_ ? kShutdownWhenScreenOffTimeoutMs
                                         : kShutdownWhenScreenOnTimeoutMs);
  shutdown_timer_.Start(FROM_HERE, timeout, this,
                        &TabletPowerButtonController::OnShutdownTimeout);
}

void TabletPowerButtonController::OnShutdownTimeout() {
  controller_->StartShutdownAnimation();
}

void TabletPowerButtonController::LockScreenIfRequired() {
  SessionController* session_controller = Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !controller_->LockRequested()) {
    session_controller->LockScreen();
  }
}

}  // namespace ash
