// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/ash_switches.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#include "ui/wm/core/compound_event_filter.h"

namespace ash {

PowerButtonController::PowerButtonController(
    LockStateController* controller)
    : power_button_down_(false),
      lock_button_down_(false),
      brightness_is_zero_(false),
      internal_display_off_and_external_display_on_(false),
      has_legacy_power_button_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuraLegacyPowerButton)),
      controller_(controller) {
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_configurator()->AddObserver(this);
#endif
}

PowerButtonController::~PowerButtonController() {
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_configurator()->RemoveObserver(this);
#endif
}

void PowerButtonController::OnScreenBrightnessChanged(double percent) {
  brightness_is_zero_ = percent <= 0.001;
}

void PowerButtonController::OnPowerButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  if (controller_->ShutdownRequested())
    return;

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451), unless an external
  // display is still on (http://crosbug.com/p/24912).
  if (brightness_is_zero_ && !internal_display_off_and_external_display_on_)
    return;

  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  if (has_legacy_power_button_) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      if (session_state_delegate->CanLockScreen() &&
          !session_state_delegate->IsScreenLocked() &&
          !controller_->LockRequested()) {
        controller_->StartLockAnimationAndLockImmediately();
      } else {
        controller_->RequestShutdown();
      }
    }
  } else {  // !has_legacy_power_button_
    if (down) {
      // If we already have a pending request to lock the screen, wait.
      if (controller_->LockRequested())
        return;

      if (session_state_delegate->CanLockScreen() &&
          !session_state_delegate->IsScreenLocked()) {
        controller_->StartLockAnimation(true);
      } else {
        controller_->StartShutdownAnimation();
      }
    } else {  // Button is up.
      if (controller_->CanCancelLockAnimation())
        controller_->CancelLockAnimation();
      else if (controller_->CanCancelShutdownAnimation())
        controller_->CancelShutdownAnimation();
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  if (!session_state_delegate->CanLockScreen() ||
      session_state_delegate->IsScreenLocked() ||
      controller_->LockRequested() ||
      controller_->ShutdownRequested()) {
    return;
  }

  // Give the power button precedence over the lock button (we don't expect both
  // buttons to be present, so this is just making sure that we don't do
  // something completely stupid if that assumption changes later).
  if (power_button_down_)
    return;

  if (down)
    controller_->StartLockAnimation(false);
  else
    controller_->CancelLockAnimation();
}

#if defined(OS_CHROMEOS)
void PowerButtonController::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  bool internal_display_off = false;
  bool external_display_on = false;
  for (size_t i = 0; i < display_states.size(); ++i) {
    const ui::DisplayConfigurator::DisplayState& state = display_states[i];
    if (state.display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!state.display->current_mode())
        internal_display_off = true;
    } else if (state.display->current_mode()) {
      external_display_on = true;
    }
  }
  internal_display_off_and_external_display_on_ =
      internal_display_off && external_display_on;
}
#endif

}  // namespace ash
