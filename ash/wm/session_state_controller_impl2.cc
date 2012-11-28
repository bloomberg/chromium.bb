// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl2.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

SessionStateControllerImpl2::TestApi::TestApi(
    SessionStateControllerImpl2* controller)
    : controller_(controller) {
}

SessionStateControllerImpl2::TestApi::~TestApi() {
}

SessionStateControllerImpl2::SessionStateControllerImpl2()
    : login_status_(user::LOGGED_IN_NONE),
      system_is_locked_(false),
      shutting_down_(false),
      shutdown_after_lock_(false) {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

SessionStateControllerImpl2::~SessionStateControllerImpl2() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

void SessionStateControllerImpl2::OnLoginStateChanged(
    user::LoginStatus status) {
  if (status != user::LOGGED_IN_LOCKED)
    login_status_ = status;
  system_is_locked_ = (status == user::LOGGED_IN_LOCKED);
}

void SessionStateControllerImpl2::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = ash::Shell::GetInstance();
    shell->env_filter()->set_cursor_hidden_by_filter(false);
    shell->cursor_manager()->ShowCursor(false);
    animator_->StartGlobalAnimation(
        internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
        internal::SessionStateAnimator::ANIMATION_SPEED_FAST);
  }
}

void SessionStateControllerImpl2::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (system_is_locked_ == locked))
    return;

  system_is_locked_ = locked;

  if (locked) {
    base::Callback<void(void)> callback =
        base::Bind(&SessionStateControllerImpl2::OnLockScreenAnimationFinished,
        base::Unretained(this));
    animator_->StartAnimationWithCallback(
        internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN,
        internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
        callback);
    lock_timer_.Stop();
    lock_fail_timer_.Stop();
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
        internal::SessionStateAnimator::LAUNCHER,
        internal::SessionStateAnimator::ANIMATION_DROP,
        internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  }
}

void SessionStateControllerImpl2::SetLockScreenDisplayedCallback(
    base::Closure& callback) {
  lock_screen_displayed_callback_ = callback;
}

void SessionStateControllerImpl2::OnLockScreenAnimationFinished() {
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(
          SessionStateObserver::EVENT_LOCK_ANIMATION_FINISHED));
  if (!lock_screen_displayed_callback_.is_null()) {
    lock_screen_displayed_callback_.Run();
    lock_screen_displayed_callback_.Reset();
  }
  if (shutdown_after_lock_) {
    shutdown_after_lock_ = false;
    StartLockToShutdownTimer();
  }
}

void SessionStateControllerImpl2::OnStartingLock() {
  if (shutting_down_ || system_is_locked_)
    return;

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
}

void SessionStateControllerImpl2::StartLockAnimationAndLockImmediately() {
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));
  OnLockTimeout();
}

void SessionStateControllerImpl2::StartLockAnimation(bool shutdown_after_lock) {
  shutdown_after_lock_ = shutdown_after_lock;

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(
          SessionStateObserver::EVENT_PRELOCK_ANIMATION_STARTED));
  StartLockTimer();
}

void SessionStateControllerImpl2::StartShutdownAnimation() {
  animator_->StartGlobalAnimation(
      internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartPreShutdownAnimationTimer();
}

bool SessionStateControllerImpl2::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool SessionStateControllerImpl2::ShutdownRequested() {
  return shutting_down_;
}

bool SessionStateControllerImpl2::CanCancelLockAnimation() {
  return lock_timer_.IsRunning();
}

void SessionStateControllerImpl2::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_DROP,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  lock_timer_.Stop();
}

bool SessionStateControllerImpl2::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         shutdown_after_lock_ ||
         lock_to_shutdown_timer_.IsRunning();
}

void SessionStateControllerImpl2::CancelShutdownAnimation() {
  if (!CanCancelShutdownAnimation())
    return;
  if (lock_to_shutdown_timer_.IsRunning()) {
    lock_to_shutdown_timer_.Stop();
    return;
  }
  if (shutdown_after_lock_) {
    shutdown_after_lock_ = false;
    return;
  }
  animator_->StartGlobalAnimation(
      internal::SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
      internal::SessionStateAnimator::ANIMATION_SPEED_REVERT);
  pre_shutdown_timer_.Stop();
}

void SessionStateControllerImpl2::RequestShutdown() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateControllerImpl2::RequestShutdownImpl() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->env_filter()->set_cursor_hidden_by_filter(false);
  shell->cursor_manager()->ShowCursor(false);

  animator_->StartGlobalAnimation(
      internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartRealShutdownTimer();
}

void SessionStateControllerImpl2::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  if(Shell::GetInstance() && Shell::GetInstance()->delegate())
    Shell::GetInstance()->delegate()->Exit();
}

void SessionStateControllerImpl2::StartLockTimer() {
  lock_timer_.Stop();
  lock_timer_.Start(
      FROM_HERE,
      animator_->GetDuration(
          internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE),
      this, &SessionStateControllerImpl2::OnLockTimeout);
}

void SessionStateControllerImpl2::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &SessionStateControllerImpl2::OnLockFailTimeout);
}

void SessionStateControllerImpl2::OnLockFailTimeout() {
  DCHECK(!system_is_locked_);
  // Undo lock animation.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER |
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_DROP,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
}

void SessionStateControllerImpl2::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
      this, &SessionStateControllerImpl2::OnLockToShutdownTimeout);
}


void SessionStateControllerImpl2::OnLockToShutdownTimeout() {
  DCHECK(system_is_locked_);
  StartShutdownAnimation();
}

void SessionStateControllerImpl2::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &SessionStateControllerImpl2::OnPreShutdownAnimationTimeout);
}

void SessionStateControllerImpl2::OnPreShutdownAnimationTimeout() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateControllerImpl2::StartRealShutdownTimer() {
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kShutdownRequestDelayMs);
  duration += animator_->GetDuration(
      internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  real_shutdown_timer_.Start(
      FROM_HERE,
      duration,
      this, &SessionStateControllerImpl2::OnRealShutdownTimeout);
}

void SessionStateControllerImpl2::OnRealShutdownTimeout() {
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

void SessionStateControllerImpl2::OnLockScreenHide(
  base::Callback<void(void)>& callback) {
  animator_->StartAnimationWithCallback(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      callback);
}

}  // namespace ash
