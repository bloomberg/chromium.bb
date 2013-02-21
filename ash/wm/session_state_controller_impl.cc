// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl.h"

#include "ash/ash_switches.h"
#include "ash/cancel_mode.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/views/corewm/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

SessionStateControllerImpl::TestApi::TestApi(
    SessionStateControllerImpl* controller)
    : controller_(controller) {
}

SessionStateControllerImpl::TestApi::~TestApi() {
}

SessionStateControllerImpl::SessionStateControllerImpl()
    : login_status_(user::LOGGED_IN_NONE),
      system_is_locked_(false),
      shutting_down_(false),
      shutdown_after_lock_(false) {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

SessionStateControllerImpl::~SessionStateControllerImpl() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

void SessionStateControllerImpl::OnLoginStateChanged(user::LoginStatus status) {
  if (status != user::LOGGED_IN_LOCKED)
    login_status_ = status;
  system_is_locked_ = (status == user::LOGGED_IN_LOCKED);
}

void SessionStateControllerImpl::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = ash::Shell::GetInstance();
    shell->env_filter()->set_cursor_hidden_by_filter(false);
    shell->cursor_manager()->HideCursor();
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
}

void SessionStateControllerImpl::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (system_is_locked_ == locked))
    return;

  system_is_locked_ = locked;

  if (locked) {
    animator_->StartAnimation(
        internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_FADE_IN,
        internal::SessionStateAnimator::ANIMATION_SPEED_SHOW_LOCK_SCREEN);
    DispatchCancelMode();
    FOR_EACH_OBSERVER(SessionStateObserver, observers_,
        OnSessionStateEvent(
            SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));
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
        internal::SessionStateAnimator::ANIMATION_RESTORE,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
}

void SessionStateControllerImpl::OnStartingLock() {
  if (shutting_down_ || system_is_locked_)
    return;

  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_FULL_CLOSE,
      internal::SessionStateAnimator::ANIMATION_SPEED_FAST);

  DispatchCancelMode();
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));

  // Hide the screen locker containers so we can make them fade in later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
}

void SessionStateControllerImpl::StartLockAnimationAndLockImmediately() {
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  DispatchCancelMode();
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));
  OnLockTimeout();
}

void SessionStateControllerImpl::StartLockAnimation(bool shutdown_after_lock) {
  shutdown_after_lock_ = shutdown_after_lock;

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  DispatchCancelMode();
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(
          SessionStateObserver::EVENT_PRELOCK_ANIMATION_STARTED));
  StartLockTimer();
}

void SessionStateControllerImpl::StartShutdownAnimation() {
  animator_->StartAnimation(
      internal::SessionStateAnimator::kAllContainersMask,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  StartPreShutdownAnimationTimer();
}

bool SessionStateControllerImpl::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool SessionStateControllerImpl::ShutdownRequested() {
  return shutting_down_;
}

bool SessionStateControllerImpl::CanCancelLockAnimation() {
  return lock_timer_.IsRunning();
}

void SessionStateControllerImpl::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE,
      internal::SessionStateAnimator::ANIMATION_SPEED_REVERT);
  lock_timer_.Stop();
}

bool SessionStateControllerImpl::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         shutdown_after_lock_ ||
         lock_to_shutdown_timer_.IsRunning();
}

void SessionStateControllerImpl::CancelShutdownAnimation() {
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

  if (system_is_locked_) {
    // If we've already started shutdown transition at lock screen
    // desktop background needs to be restored immediately.
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_RESTORE,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE,
        internal::SessionStateAnimator::ANIMATION_SPEED_REVERT);
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE,
        internal::SessionStateAnimator::ANIMATION_SPEED_REVERT);
  }
  pre_shutdown_timer_.Stop();
}

void SessionStateControllerImpl::RequestShutdown() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateControllerImpl::RequestShutdownImpl() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->env_filter()->set_cursor_hidden_by_filter(false);
  shell->cursor_manager()->HideCursor();

  if (login_status_ != user::LOGGED_IN_NONE) {
    // Hide the other containers before starting the animation.
    // ANIMATION_FULL_CLOSE will make the screen locker windows partially
    // transparent, and we don't want the other windows to show through.
    animator_->StartAnimation(
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
        internal::SessionStateAnimator::LAUNCHER,
        internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_FULL_CLOSE,
        internal::SessionStateAnimator::ANIMATION_SPEED_FAST);
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_FULL_CLOSE,
        internal::SessionStateAnimator::ANIMATION_SPEED_FAST);
  }
  StartRealShutdownTimer();
}

void SessionStateControllerImpl::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  Shell::GetInstance()->delegate()->Exit();
}

void SessionStateControllerImpl::StartLockTimer() {
  lock_timer_.Stop();
  lock_timer_.Start(
      FROM_HERE,
      animator_->GetDuration(
          internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE),
      this, &SessionStateControllerImpl::OnLockTimeout);
}

void SessionStateControllerImpl::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &SessionStateControllerImpl::OnLockFailTimeout);
}

void SessionStateControllerImpl::OnLockFailTimeout() {
  DCHECK(!system_is_locked_);
  // Undo lock animation.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER |
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_RESTORE,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
}

void SessionStateControllerImpl::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
      this, &SessionStateControllerImpl::OnLockToShutdownTimeout);
}


void SessionStateControllerImpl::OnLockToShutdownTimeout() {
  DCHECK(system_is_locked_);
  StartShutdownAnimation();
}

void SessionStateControllerImpl::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &SessionStateControllerImpl::OnPreShutdownAnimationTimeout);
}

void SessionStateControllerImpl::OnPreShutdownAnimationTimeout() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateControllerImpl::StartRealShutdownTimer() {
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kShutdownRequestDelayMs);
  duration += animator_->GetDuration(
      internal::SessionStateAnimator::ANIMATION_SPEED_FAST);

  real_shutdown_timer_.Start(
      FROM_HERE,
      duration,
      this, &SessionStateControllerImpl::OnRealShutdownTimeout);
}

void SessionStateControllerImpl::OnRealShutdownTimeout() {
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

void SessionStateControllerImpl::OnLockScreenHide(base::Closure& callback) {
  callback.Run();
}

void SessionStateControllerImpl::SetLockScreenDisplayedCallback(
    base::Closure& callback) {
  NOTIMPLEMENTED();
}

}  // namespace ash
