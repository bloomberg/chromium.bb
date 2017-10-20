// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "ash/system/power/tablet_power_button_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/event.h"

namespace ash {
namespace {

// When clamshell power button behavior is forced, turn the screen off this long
// after locking is requested via the power button.
constexpr base::TimeDelta kDisplayOffAfterLockDelay =
    base::TimeDelta::FromSeconds(3);

}  // namespace

PowerButtonController::PowerButtonController()
    : lock_state_controller_(Shell::Get()->lock_state_controller()),
      tick_clock_(new base::DefaultTickClock) {
  ProcessCommandLine();
  display_controller_ =
      std::make_unique<PowerButtonDisplayController>(tick_clock_.get());
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
  Shell::Get()->display_configurator()->AddObserver(this);
  Shell::Get()->PrependPreTargetHandler(this);
}

PowerButtonController::~PowerButtonController() {
  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->display_configurator()->RemoveObserver(this);
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  power_button_down_ = down;
  if (down)
    started_lock_animation_for_power_button_down_ = false;

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451), unless an external
  // display is still on (http://crosbug.com/p/24912).
  if (brightness_is_zero_ && !internal_display_off_and_external_display_on_)
    return;

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  if (button_type_ == ButtonType::LEGACY) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      if (session_controller->CanLockScreen() &&
          !session_controller->IsUserSessionBlocked() &&
          !lock_state_controller_->LockRequested()) {
        lock_state_controller_->StartLockAnimationAndLockImmediately();
      } else {
        lock_state_controller_->RequestShutdown(ShutdownReason::POWER_BUTTON);
      }
    }
    return;
  }

  if (down) {
    // If we already have a pending request to lock the screen, wait.
    if (lock_state_controller_->LockRequested())
      return;

    if (session_controller->CanLockScreen() &&
        !session_controller->IsUserSessionBlocked()) {
      lock_state_controller_->StartLockThenShutdownAnimation(
          ShutdownReason::POWER_BUTTON);
      started_lock_animation_for_power_button_down_ = true;
    } else {
      lock_state_controller_->StartShutdownAnimation(
          ShutdownReason::POWER_BUTTON);
    }
  } else {  // Button is up.
    if (lock_state_controller_->CanCancelLockAnimation())
      lock_state_controller_->CancelLockAnimation();
    else if (lock_state_controller_->CanCancelShutdownAnimation())
      lock_state_controller_->CancelShutdownAnimation();

    // Avoid awkwardly keeping the display on at the lock screen for a long time
    // if we're forcing clamshell behavior on a convertible device, since it
    // makes it difficult to transport the device while it's in tablet mode.
    if (force_clamshell_power_button_ &&
        started_lock_animation_for_power_button_down_ &&
        (session_controller->IsScreenLocked() ||
         lock_state_controller_->LockRequested())) {
      display_off_timer_.Start(
          FROM_HERE, kDisplayOffAfterLockDelay, this,
          &PowerButtonController::ForceDisplayOffAfterLock);
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  if (!session_controller->CanLockScreen() ||
      session_controller->IsScreenLocked() ||
      lock_state_controller_->LockRequested() ||
      lock_state_controller_->ShutdownRequested()) {
    return;
  }

  // Give the power button precedence over the lock button.
  if (power_button_down_)
    return;

  if (down)
    lock_state_controller_->StartLockAnimation();
  else
    lock_state_controller_->CancelLockAnimation();
}

void PowerButtonController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->key_code() != ui::VKEY_POWER)
    display_off_timer_.Stop();
}

void PowerButtonController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  display_off_timer_.Stop();
}

void PowerButtonController::OnTouchEvent(ui::TouchEvent* event) {
  display_off_timer_.Stop();
}

void PowerButtonController::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  bool internal_display_off = false;
  bool external_display_on = false;
  for (const display::DisplaySnapshot* display : display_states) {
    if (display->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!display->current_mode())
        internal_display_off = true;
    } else if (display->current_mode()) {
      external_display_on = true;
    }
  }
  internal_display_off_and_external_display_on_ =
      internal_display_off && external_display_on;
}

void PowerButtonController::BrightnessChanged(int level, bool user_initiated) {
  brightness_is_zero_ = level == 0;
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  if (lock_state_controller_->ShutdownRequested())
    return;

  // PowerButtonDisplayController ignores power button events, so tell it to
  // stop forcing the display off if TabletPowerButtonController isn't being
  // used.
  if (down && force_clamshell_power_button_)
    display_controller_->SetDisplayForcedOff(false);

  // Handle tablet mode power button screenshot accelerator.
  if (screenshot_controller_ &&
      screenshot_controller_->OnPowerButtonEvent(down, timestamp)) {
    return;
  }

  // Handle tablet power button behavior.
  if (button_type_ == ButtonType::NORMAL && tablet_controller_) {
    tablet_controller_->OnPowerButtonEvent(down, timestamp);
    return;
  }

  // Handle clamshell power button behavior.
  OnPowerButtonEvent(down, timestamp);
}

void PowerButtonController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  // Tablet power button behavior (excepts |force_clamshell_power_button_|) and
  // power button screenshot accelerator are enabled on devices that can enter
  // tablet mode, which must have seen accelerometer data before user actions.
  if (!enable_tablet_mode_)
    return;
  if (!force_clamshell_power_button_ && !tablet_controller_) {
    tablet_controller_ = std::make_unique<TabletPowerButtonController>(
        display_controller_.get(), tick_clock_.get());
  }

  if (!screenshot_controller_) {
    screenshot_controller_ = std::make_unique<PowerButtonScreenshotController>(
        tablet_controller_.get(), tick_clock_.get(),
        force_clamshell_power_button_);
  }
}

void PowerButtonController::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(tick_clock);
  tick_clock_ = std::move(tick_clock);

  display_controller_ =
      std::make_unique<PowerButtonDisplayController>(tick_clock_.get());
}

bool PowerButtonController::TriggerDisplayOffTimerForTesting() {
  if (!display_off_timer_.IsRunning())
    return false;

  base::Closure task = display_off_timer_.user_task();
  display_off_timer_.Stop();
  task.Run();
  return true;
}

void PowerButtonController::ProcessCommandLine() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  button_type_ = cl->HasSwitch(switches::kAuraLegacyPowerButton)
                     ? ButtonType::LEGACY
                     : ButtonType::NORMAL;
  enable_tablet_mode_ = cl->HasSwitch(switches::kAshEnableTabletMode);
  force_clamshell_power_button_ =
      cl->HasSwitch(switches::kForceClamshellPowerButton);
}

void PowerButtonController::ForceDisplayOffAfterLock() {
  display_controller_->SetDisplayForcedOff(true);
}

}  // namespace ash
