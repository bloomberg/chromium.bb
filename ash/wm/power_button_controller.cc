// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_controller.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"

namespace ash {

PowerButtonController::PowerButtonController(SessionStateController* controller)
    : power_button_down_(false),
      lock_button_down_(false),
      screen_is_off_(false),
      has_legacy_power_button_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuraLegacyPowerButton)),
      controller_(controller) {
}

PowerButtonController::~PowerButtonController() {
}

void PowerButtonController::OnScreenBrightnessChanged(double percent) {
  screen_is_off_ = percent <= 0.001;
}

void PowerButtonController::OnPowerButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  if (controller_->ShutdownRequested())
    return;

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451).
  if (screen_is_off_)
    return;

  Shell* shell = Shell::GetInstance();
  if (has_legacy_power_button_) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      if (shell->CanLockScreen() && !shell->IsScreenLocked() &&
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

      if (shell->CanLockScreen() && !shell->IsScreenLocked())
        controller_->StartLockAnimation(true);
      else
        controller_->StartShutdownAnimation();
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

  Shell* shell = Shell::GetInstance();
  if (!shell->CanLockScreen() || shell->IsScreenLocked() ||
      controller_->LockRequested() || controller_->ShutdownRequested()) {
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

}  // namespace ash
