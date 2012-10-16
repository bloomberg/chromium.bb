// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller.h"

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

const int SessionStateController::kLockTimeoutMs = 400;
const int SessionStateController::kShutdownTimeoutMs = 400;
const int SessionStateController::kLockFailTimeoutMs = 4000;
const int SessionStateController::kLockToShutdownTimeoutMs = 150;
const int SessionStateController::kSlowCloseAnimMs = 400;
const int SessionStateController::kUndoSlowCloseAnimMs = 100;
const int SessionStateController::kFastCloseAnimMs = 150;
const int SessionStateController::kShutdownRequestDelayMs = 50;

SessionStateController::TestApi::TestApi(SessionStateController* controller)
    : controller_(controller) {
}

SessionStateController::TestApi::~TestApi() {
}

SessionStateController::SessionStateController()
    : login_status_(user::LOGGED_IN_NONE),
      unlocked_login_status_(user::LOGGED_IN_NONE),
      shutting_down_(false),
      shutdown_after_lock_(false),
      animator_(new internal::SessionStateAnimator()) {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

SessionStateController::~SessionStateController() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

void SessionStateController::SetDelegate(
    SessionStateControllerDelegate* delegate) {
  delegate_.reset(delegate);
}

void SessionStateController::OnLoginStateChanged(user::LoginStatus status) {
  login_status_ = status;
  unlocked_login_status_ = user::LOGGED_IN_NONE;
}

void SessionStateController::OnAppTerminating() {
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

void SessionStateController::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (IsLocked()) == locked)
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

    if (shutdown_after_lock_) {
      shutdown_after_lock_ = false;
      StartLockToShutdownTimer();
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

void SessionStateController::OnStartingLock() {
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

void SessionStateController::StartLockAnimationAndLockImmediately() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);
  OnLockTimeout();
}

void SessionStateController::StartLockAnimation() {
  shutdown_after_lock_ = true;

  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);
  StartLockTimer();
}

void SessionStateController::StartShutdownAnimation() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::kAllContainersMask,
      internal::SessionStateAnimator::ANIMATION_SLOW_CLOSE);

  StartPreShutdownAnimationTimer();
}

bool SessionStateController::IsEligibleForLock() {
  return IsLoggedInAsNonGuest() && !IsLocked() && !LockRequested();
}

bool SessionStateController::IsLocked() {
  return login_status_ == user::LOGGED_IN_LOCKED;
}

bool SessionStateController::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool SessionStateController::ShutdownRequested() {
  return shutting_down_;
}

bool SessionStateController::CanCancelLockAnimation() {
  return lock_timer_.IsRunning();
}

void SessionStateController::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animator_->StartAnimation(
      internal::SessionStateAnimator::kAllContainersMask,
      internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
  animator_->ScheduleDropBlackLayer();
  lock_timer_.Stop();
}

void SessionStateController::CancelLockWithOtherAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
  animator_->ScheduleDropBlackLayer();
  lock_timer_.Stop();
}

bool SessionStateController::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         lock_to_shutdown_timer_.IsRunning();
}

void SessionStateController::CancelShutdownAnimation() {
  if (!CanCancelShutdownAnimation())
    return;
  if (lock_to_shutdown_timer_.IsRunning()) {
    lock_to_shutdown_timer_.Stop();
    return;
  }

  if (login_status_ == user::LOGGED_IN_LOCKED) {
    // If we've already started shutdown transition at lock screen
    // desktop background needs to be restored immediately.
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_RESTORE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
    animator_->ScheduleDropBlackLayer();
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_SLOW_CLOSE);
    animator_->ScheduleDropBlackLayer();
  }
  pre_shutdown_timer_.Stop();
}

void SessionStateController::RequestShutdown() {
  if (!shutting_down_)
    animator_->ShowBlackLayer();
    RequestShutdownImpl();
}

void SessionStateController::RequestShutdownImpl() {
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
  StartRealShutdownTimer();
}

void SessionStateController::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  if(Shell::GetInstance() && Shell::GetInstance()->delegate())
    Shell::GetInstance()->delegate()->Exit();
}

bool SessionStateController::IsLoggedInAsNonGuest() const {
  // TODO(mukai): think about kiosk mode.
  return (login_status_ != user::LOGGED_IN_NONE) &&
         (login_status_ != user::LOGGED_IN_GUEST);
}

void SessionStateController::StartLockTimer() {
  lock_timer_.Stop();
  lock_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kSlowCloseAnimMs),
                    this, &SessionStateController::OnLockTimeout);
}

void SessionStateController::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &SessionStateController::OnLockFailTimeout);
}

void SessionStateController::OnLockFailTimeout() {
  DCHECK_NE(login_status_, user::LOGGED_IN_LOCKED);
  // Undo lock animation.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER |
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_RESTORE);
  animator_->DropBlackLayer();
}

void SessionStateController::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
      this, &SessionStateController::OnLockToShutdownTimeout);
}


void SessionStateController::OnLockToShutdownTimeout() {
  DCHECK_EQ(login_status_, user::LOGGED_IN_LOCKED);
  StartShutdownAnimation();
}

void SessionStateController::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &SessionStateController::OnPreShutdownAnimationTimeout);
}

void SessionStateController::OnPreShutdownAnimationTimeout() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateController::StartRealShutdownTimer() {
  real_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kFastCloseAnimMs +
          kShutdownRequestDelayMs),
      this, &SessionStateController::OnRealShutdownTimeout);
}

void SessionStateController::OnRealShutdownTimeout() {
  DCHECK(shutting_down_);
#if defined(OS_CHROMEOS)
  if (!base::chromeos::IsRunningOnChromeOS()) {
    ShellDelegate* delegate = Shell::GetInstance()->delegate();
    if (delegate) {
      delegate->Exit();
      return;
    }
  }
#endif
  delegate_->RequestShutdown();
}

}  // namespace ash
