// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/tablet_power_button_controller.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
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
constexpr int kShutdownTimeoutMs = 500;

// Amount of time to delay locking screen after display is forced off.
constexpr int kLockScreenTimeoutMs = 1000;

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

bool TabletPowerButtonController::TestApi::LockScreenTimerIsRunning() const {
  return controller_->lock_screen_timer_.IsRunning();
}

void TabletPowerButtonController::TestApi::TriggerLockScreenTimeout() {
  DCHECK(LockScreenTimerIsRunning());
  controller_->OnLockScreenTimeout();
  controller_->lock_screen_timer_.Stop();
}

TabletPowerButtonController::TabletPowerButtonController(
    LockStateController* controller)
    : tick_clock_(new base::DefaultTickClock()),
      last_resume_time_(base::TimeTicks()),
      force_off_on_button_up_(true),
      controller_(controller),
      weak_ptr_factory_(this) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  WmShell::Get()->AddShellObserver(this);
  // TODO(mash): Provide a way for this class to observe stylus events:
  // http://crbug.com/682460
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->AddObserver(this);
  Shell::GetInstance()->PrependPreTargetHandler(this);

  GetInitialBacklightsForcedOff();
}

TabletPowerButtonController::~TabletPowerButtonController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
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
    lock_screen_timer_.Stop();
    StartShutdownTimer();
  } else {
    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_ && force_off_on_button_up_) {
        SetDisplayForcedOff(true);
        LockScreenIfRequired();
      }
    }

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

  if (!IsTabletModeActive() && backlights_forced_off_) {
    SetDisplayForcedOff(false);
    lock_screen_timer_.Stop();
  }
}

void TabletPowerButtonController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_) {
    SetDisplayForcedOff(false);
    lock_screen_timer_.Stop();
  }
}

void TabletPowerButtonController::OnStylusStateChanged(ui::StylusState state) {
  if (IsTabletModeSupported() && state == ui::StylusState::REMOVED &&
      backlights_forced_off_) {
    SetDisplayForcedOff(false);
    lock_screen_timer_.Stop();
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

  ShellDelegate* delegate = WmShell::Get()->delegate();
  delegate->SetTouchscreenEnabledInPrefs(!forced_off,
                                         true /* use_local_state */);
  delegate->UpdateTouchscreenStatusFromPrefs();

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
    lock_screen_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kLockScreenTimeoutMs),
        this, &TabletPowerButtonController::OnLockScreenTimeout);
  }
}

void TabletPowerButtonController::OnLockScreenTimeout() {
  WmShell::Get()->GetSessionStateDelegate()->LockScreen();
}

}  // namespace ash
