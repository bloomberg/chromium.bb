// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shutdown_reason.h"
#include "ash/system/audio/tray_audio.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/tablet_power_button_controller.h"
#include "ash/system/tray/system_tray.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/wm/core/compound_event_filter.h"

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

  if (lock_state_controller_->ShutdownRequested())
    return;

  bool should_take_screenshot = down && volume_down_pressed_ &&
                                Shell::Get()
                                    ->tablet_mode_controller()
                                    ->IsTabletModeWindowManagerEnabled();

  if (!has_legacy_power_button_ && !should_take_screenshot &&
      tablet_controller_) {
    tablet_controller_->OnPowerButtonEvent(down, timestamp);
    return;
  }

  // PowerButtonDisplayController ignores power button events, so tell it to
  // stop forcing the display off if TabletPowerButtonController isn't being
  // used.
  if (down && force_clamshell_power_button_)
    display_controller_->SetDisplayForcedOff(false);

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451), unless an external
  // display is still on (http://crosbug.com/p/24912).
  if (brightness_is_zero_ && !internal_display_off_and_external_display_on_)
    return;

  // Take screenshot on power button down plus volume down when in touch view.
  if (should_take_screenshot) {
    SystemTray* system_tray = Shell::Get()->GetPrimarySystemTray();
    if (system_tray && system_tray->GetTrayAudio())
      system_tray->GetTrayAudio()->HideDetailedView(false);

    Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
        TAKE_SCREENSHOT);

    // Restore volume.
    chromeos::CrasAudioHandler* audio_handler =
        chromeos::CrasAudioHandler::Get();
    audio_handler->SetOutputVolumePercentWithoutNotifyingObservers(
        volume_percent_before_screenshot_,
        chromeos::CrasAudioHandler::VOLUME_CHANGE_MAXIMIZE_MODE_SCREENSHOT);
    return;
  }

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  if (has_legacy_power_button_) {
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
  if (event->key_code() == ui::VKEY_VOLUME_DOWN) {
    volume_down_pressed_ = event->type() == ui::ET_KEY_PRESSED;
    if (!event->is_repeat()) {
      chromeos::CrasAudioHandler* audio_handler =
          chromeos::CrasAudioHandler::Get();
      volume_percent_before_screenshot_ =
          audio_handler->GetOutputVolumePercent();
    }
  }
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
  OnPowerButtonEvent(down, timestamp);
}

void PowerButtonController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  if (force_clamshell_power_button_ || tablet_controller_)
    return;
  tablet_controller_.reset(new TabletPowerButtonController(
      display_controller_.get(), tick_clock_.get()));
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
  has_legacy_power_button_ = cl->HasSwitch(switches::kAuraLegacyPowerButton);
  force_clamshell_power_button_ =
      cl->HasSwitch(switches::kForceClamshellPowerButton);
}

void PowerButtonController::ForceDisplayOffAfterLock() {
  display_controller_->SetDisplayForcedOff(true);
}

}  // namespace ash
