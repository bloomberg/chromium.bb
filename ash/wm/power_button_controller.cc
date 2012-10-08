// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

namespace {

// Amount of time that the power button needs to be held before we lock the
// screen.
const int kLockTimeoutMs = 400;

// Amount of time that the power button needs to be held before we shut down.
const int kShutdownTimeoutMs = 400;

// Amount of time to wait for our lock requests to be honored before giving up.
const int kLockFailTimeoutMs = 4000;

// When the button has been held continuously from the unlocked state, amount of
// time that we wait after the screen locker window is shown before starting the
// pre-shutdown animation.
const int kLockToShutdownTimeoutMs = 150;

// Amount of time taken to scale the snapshot of the screen down to a
// slightly-smaller size once the user starts holding the power button.  Used
// for both the pre-lock and pre-shutdown animations.
const int kSlowCloseAnimMs = 400;

// Amount of time taken to scale the snapshot of the screen back to its original
// size when the button is released.
const int kUndoSlowCloseAnimMs = 100;

// Amount of time taken to scale the snapshot down to a point in the center of
// the screen once the screen has been locked or we've been notified that the
// system is shutting down.
const int kFastCloseAnimMs = 150;

// Additional time (beyond kFastCloseAnimMs) to wait after starting the
// fast-close shutdown animation before actually requesting shutdown, to give
// the animation time to finish.
const int kShutdownRequestDelayMs = 50;

}  // namespace

PowerButtonController::TestApi::TestApi(PowerButtonController* controller)
    : controller_(controller),
      animator_api_(new internal::SessionStateAnimator::TestApi(
                        controller->animator_.get())) {
}

PowerButtonController::TestApi::~TestApi() {
}

PowerButtonController::PowerButtonController()
    : login_status_(user::LOGGED_IN_NONE),
      unlocked_login_status_(user::LOGGED_IN_NONE),
      power_button_down_(false),
      lock_button_down_(false),
      screen_is_off_(false),
      shutting_down_(false),
      has_legacy_power_button_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuraLegacyPowerButton)),
      animator_(new internal::SessionStateAnimator()) {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

PowerButtonController::~PowerButtonController() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

void PowerButtonController::OnLoginStateChanged(user::LoginStatus status) {
  login_status_ = status;
  unlocked_login_status_ = user::LOGGED_IN_NONE;
}

void PowerButtonController::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = ash::Shell::GetInstance();
    shell->env_filter()->set_cursor_hidden_by_filter(false);
    shell->cursor_manager()->ShowCursor(false);
    animator_->ShowBlackLayer();
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_HIDE);
  }
}

void PowerButtonController::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (login_status_ == user::LOGGED_IN_LOCKED) == locked)
    return;

  if (!locked && login_status_ == user::LOGGED_IN_LOCKED) {
    login_status_ = unlocked_login_status_;
    unlocked_login_status_ = user::LOGGED_IN_NONE;
  } else {
    unlocked_login_status_ = login_status_;
    login_status_ = user::LOGGED_IN_LOCKED;
  }

  if (locked) {
    animator_->StartAnimation(
        internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_FADE_IN);
    lock_timer_.Stop();
    lock_fail_timer_.Stop();

    if (!has_legacy_power_button_ && power_button_down_) {
      lock_to_shutdown_timer_.Stop();
      lock_to_shutdown_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
          this, &PowerButtonController::OnLockToShutdownTimeout);
    }
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND |
        internal::SessionStateAnimator::LAUNCHER |
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_RESTORE);
    animator_->DropBlackLayer();
  }
}

void PowerButtonController::OnScreenBrightnessChanged(double percent) {
  screen_is_off_ = percent <= 0.001;
}

void PowerButtonController::OnStartingLock() {
  if (shutting_down_ || login_status_ == user::LOGGED_IN_LOCKED)
    return;

  // Ensure that the black layer is visible -- if the screen was locked via
  // the wrench menu, we won't have already shown the black background
  // as part of the slow-close animation.
  animator_->ShowBlackLayer();

  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_HIDE);

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_FAST_CLOSE);

  // Hide the screen locker containers so we can make them fade in later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE);
}

void PowerButtonController::OnPowerButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  if (shutting_down_)
    return;

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451).
  if (screen_is_off_)
    return;

  if (has_legacy_power_button_) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      animator_->ShowBlackLayer();
      if (LoggedInAsNonGuest() && login_status_ != user::LOGGED_IN_LOCKED) {
        animator_->StartAnimation(
            internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);
        OnLockTimeout();
      } else {
        OnShutdownTimeout();
      }
    }
  } else {  // !has_legacy_power_button_
    if (down) {
      // If we already have a pending request to lock the screen, wait.
      if (lock_fail_timer_.IsRunning())
        return;

      if (LoggedInAsNonGuest() && login_status_ != user::LOGGED_IN_LOCKED)
        StartLockTimer();
      else
        StartShutdownTimer();
    } else {  // Button is up.
      if (lock_timer_.IsRunning() || shutdown_timer_.IsRunning()) {
        if (login_status_ == user::LOGGED_IN_LOCKED) {
          // If we've already started shutdown transition at lock screen
          // desktop background needs to be restored immediately.
          animator_->StartAnimation(
              internal::SessionStateAnimator::DESKTOP_BACKGROUND,
              internal::SessionStateAnimator::ANIMATION_RESTORE);
        }
        animator_->StartAnimation(
            (login_status_ == user::LOGGED_IN_LOCKED) ?
            internal::SessionStateAnimator::kAllLockScreenContainersMask :
            internal::SessionStateAnimator::kAllContainersMask,
            internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
      }

      // Drop the black layer after the undo animation finishes.
      if (lock_timer_.IsRunning() ||
          (shutdown_timer_.IsRunning() && !LoggedInAsNonGuest())) {
        animator_->ScheduleDropBlackLayer();
      }

      lock_timer_.Stop();
      shutdown_timer_.Stop();
      lock_to_shutdown_timer_.Stop();
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  if (shutting_down_ || !LoggedInAsNonGuest())
    return;

  // Bail if we're already locked or are in the process of locking.  Also give
  // the power button precedence over the lock button (we don't expect both
  // buttons to be present, so this is just making sure that we don't do
  // something completely stupid if that assumption changes later).
  if (login_status_ == user::LOGGED_IN_LOCKED ||
      lock_fail_timer_.IsRunning() || power_button_down_)
    return;

  if (down) {
    StartLockTimer();
  } else {
    if (lock_timer_.IsRunning()) {
      animator_->StartAnimation(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
      animator_->ScheduleDropBlackLayer();
      lock_timer_.Stop();
    }
  }
}

void PowerButtonController::RequestShutdown() {
  if (!shutting_down_)
    StartShutdownAnimationAndRequestShutdown();
}

void PowerButtonController::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  if(Shell::GetInstance() && Shell::GetInstance()->delegate())
    Shell::GetInstance()->delegate()->Exit();
}

bool PowerButtonController::LoggedInAsNonGuest() const {
  if (login_status_ == user::LOGGED_IN_NONE)
    return false;
  if (login_status_ == user::LOGGED_IN_GUEST)
    return false;
  // TODO(mukai): think about kiosk mode.
  return true;
}

void PowerButtonController::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &PowerButtonController::OnLockFailTimeout);
}

void PowerButtonController::OnLockFailTimeout() {
  DCHECK_NE(login_status_, user::LOGGED_IN_LOCKED);
  LOG(ERROR) << "Screen lock request timed out";
  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER |
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_RESTORE);
  animator_->DropBlackLayer();
}

void PowerButtonController::OnLockToShutdownTimeout() {
  DCHECK_EQ(login_status_, user::LOGGED_IN_LOCKED);
  StartShutdownTimer();
}

void PowerButtonController::OnShutdownTimeout() {
  if (!shutting_down_)
    StartShutdownAnimationAndRequestShutdown();
}

void PowerButtonController::OnRealShutdownTimeout() {
  DCHECK(shutting_down_);
#if defined(OS_CHROMEOS)
  if (!base::chromeos::IsRunningOnChromeOS()) {
    ShellDelegate* delegate = Shell::GetInstance()->delegate();
    if (delegate)
      delegate->Exit();
  }
#endif
  delegate_->RequestShutdown();
}

void PowerButtonController::StartLockTimer() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);
  lock_timer_.Stop();
  lock_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kSlowCloseAnimMs),
                    this, &PowerButtonController::OnLockTimeout);
}

void PowerButtonController::StartShutdownTimer() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::kAllContainersMask,
      internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);
  shutdown_timer_.Stop();
  shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &PowerButtonController::OnShutdownTimeout);
}

void PowerButtonController::StartShutdownAnimationAndRequestShutdown() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->env_filter()->set_cursor_hidden_by_filter(false);
  shell->cursor_manager()->ShowCursor(false);

  animator_->ShowBlackLayer();
  if (login_status_ != user::LOGGED_IN_NONE) {
    // Hide the other containers before starting the animation.
    // ANIMATION_FAST_CLOSE will make the screen locker windows partially
    // transparent, and we don't want the other windows to show through.
    animator_->StartAnimation(
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
        internal::SessionStateAnimator::LAUNCHER,
        internal::SessionStateAnimator::ANIMATION_HIDE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_FAST_CLOSE);
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_FAST_CLOSE);
  }

  real_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          kFastCloseAnimMs + kShutdownRequestDelayMs),
      this, &PowerButtonController::OnRealShutdownTimeout);
}

}  // namespace ash
