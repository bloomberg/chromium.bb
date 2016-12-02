// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tablet_power_button_controller.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// Amount of time the power button must be held to start the pre-shutdown
// animation when in tablet mode.
constexpr int kShutdownTimeoutMs = 1000;

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
constexpr int kIgnorePowerButtonAfterResumeMs = 2000;

// Returns true if device is a convertible/tablet device or has
// kAshEnableTouchViewTesting in test, otherwise false.
bool IsTabletModeSupported() {
  MaximizeModeController* maximize_mode_controller =
      WmShell::Get()->maximize_mode_controller();
  return maximize_mode_controller &&
         maximize_mode_controller->CanEnterMaximizeMode();
}

// Returns true if device is currently in tablet/maximize mode, otherwise false.
bool IsTabletModeActive() {
  MaximizeModeController* maximize_mode_controller =
      WmShell::Get()->maximize_mode_controller();
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
      last_resume_time_(base::TimeTicks()),
      controller_(controller),
      weak_ptr_factory_(this) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  Shell::GetInstance()->PrependPreTargetHandler(this);

  GetInitialBacklightsForcedOff();
}

TabletPowerButtonController::~TabletPowerButtonController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

bool TabletPowerButtonController::ShouldHandlePowerButtonEvents() const {
  return IsTabletModeSupported();
}

void TabletPowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  // When the system resumes in response to the power button being pressed,
  // Chrome receives powerd's SuspendDone signal and notification that the
  // backlight has been turned back on before seeing the power button events
  // that woke the system. Ignore events just after resuming to ensure that we
  // don't turn the screen off in response to the events.
  if (timestamp - last_resume_time_ <=
      base::TimeDelta::FromMilliseconds(kIgnorePowerButtonAfterResumeMs)) {
    // If backlights are forced off, stop forcing off because resuming system
    // doesn't handle this.
    if (down && backlights_forced_off_)
      SetBacklightsForcedOff(false);
    return;
  }

  if (down) {
    screen_off_when_power_button_down_ = brightness_level_is_zero_;
    SetBacklightsForcedOff(false);
    StartShutdownTimer();
  } else {
    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_) {
        SetBacklightsForcedOff(true);
        LockScreenIfRequired();
      }
    }
    screen_off_when_power_button_down_ = false;

    // When power button is released, cancel shutdown animation whenever it is
    // still cancellable.
    if (controller_->CanCancelShutdownAnimation())
      controller_->CancelShutdownAnimation();
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

void TabletPowerButtonController::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore key events generated by the power button since power button activity
  // is already handled by OnPowerButtonEvent().
  if (event->key_code() == ui::VKEY_POWER)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetBacklightsForcedOff(false);
}

void TabletPowerButtonController::OnMouseEvent(ui::MouseEvent* event) {
  ui::EventPointerType pointer_type = event->pointer_details().pointer_type;

  if (pointer_type != ui::EventPointerType::POINTER_TYPE_MOUSE ||
      (event->flags() & ui::EF_IS_SYNTHESIZED)) {
    return;
  }

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetBacklightsForcedOff(false);
}

void TabletPowerButtonController::OnStylusStateChanged(ui::StylusState state) {
  if (IsTabletModeSupported() && state == ui::StylusState::REMOVED &&
      backlights_forced_off_) {
    SetBacklightsForcedOff(false);
  }
}

void TabletPowerButtonController::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(tick_clock);
  tick_clock_ = std::move(tick_clock);
}

void TabletPowerButtonController::SetBacklightsForcedOff(bool forced_off) {
  if (backlights_forced_off_ == forced_off)
    return;

  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(forced_off);
  backlights_forced_off_ = forced_off;

  // Send an a11y alert.
  WmShell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
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
}

void TabletPowerButtonController::StartShutdownTimer() {
  shutdown_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
                        this, &TabletPowerButtonController::OnShutdownTimeout);
}

void TabletPowerButtonController::OnShutdownTimeout() {
  controller_->StartShutdownAnimation();
}

void TabletPowerButtonController::LockScreenIfRequired() {
  SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  if (session_state_delegate->ShouldLockScreenAutomatically() &&
      session_state_delegate->CanLockScreen() &&
      !session_state_delegate->IsUserSessionBlocked() &&
      !controller_->LockRequested()) {
    session_state_delegate->LockScreen();
  }
}

}  // namespace ash
