// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl2.h"

#include "ash/ash_switches.h"
#include "ash/cancel_mode.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/timer.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/corewm/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

namespace {

aura::Window* GetBackground() {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  return Shell::GetContainer(root_window,
      internal::kShellWindowId_DesktopBackgroundContainer);
}

bool IsBackgroundHidden() {
  return !GetBackground()->IsVisible();
}

void ShowBackground() {
  ui::ScopedLayerAnimationSettings settings(
      GetBackground()->layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetBackground()->Show();
}

void HideBackground() {
  ui::ScopedLayerAnimationSettings settings(
      GetBackground()->layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetBackground()->Hide();
}

// This observer is intended to use in cases when some action has to be taken
// once some animation successfully completes (i.e. it was not aborted).
// Observer will count a number of sequences it is attached to, and a number of
// finished sequences (either Ended or Aborted). Once these two numbers are
// equal, observer will delete itself, calling callback passed to constructor if
// there were no aborted animations.
// This way it can be either used to wait for some animation to be finished in
// multiple layers, to wait once a sequence of animations is finished in one
// layer or the mixture of both.
class AnimationFinishedObserver : public ui::LayerAnimationObserver {
 public:
  explicit AnimationFinishedObserver(base::Closure &callback)
      : callback_(callback),
        sequences_attached_(0),
        sequences_completed_(0),
        paused_(false) {
  }

  // Pauses observer: no checks will be made while paused. It can be used when
  // a sequence has some immediate animations in the beginning, and for
  // animations that can be tested with flag that makes all animations
  // immediate.
  void Pause() {
    paused_ = true;
  }

  // Unpauses observer. It does a check and calls callback if conditions are
  // met.
  void Unpause() {
    if (!paused_)
      return;
    paused_ = false;
    if (sequences_completed_ == sequences_attached_) {
      callback_.Run();
      delete this;
    }
  }

 private:
  virtual ~AnimationFinishedObserver() {
  }

  // LayerAnimationObserver implementation
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    sequences_completed_++;
    if ((sequences_completed_ == sequences_attached_) && !paused_) {
      callback_.Run();
      delete this;
    }
  }

  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    sequences_completed_++;
    if ((sequences_completed_ == sequences_attached_) && !paused_)
      delete this;
  }

  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
  }

  virtual void OnAttachedToSequence(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    LayerAnimationObserver::OnAttachedToSequence(sequence);
    sequences_attached_++;
  }

  // Callback to be called.
  base::Closure callback_;

  // Number of sequences this observer was attached to.
  int sequences_attached_;

  // Number of sequences either ended or aborted.
  int sequences_completed_;

  bool paused_;

  DISALLOW_COPY_AND_ASSIGN(AnimationFinishedObserver);
};

} // namespace

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
      shutdown_after_lock_(false),
      animating_lock_(false),
      can_cancel_lock_animation_(false) {
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
  // This is also the case when the user signs off.
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

void SessionStateControllerImpl2::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (system_is_locked_ == locked))
    return;

  system_is_locked_ = locked;

  if (locked) {
    StartPostLockAnimation();
    lock_fail_timer_.Stop();
  } else {
    StartUnlockAnimationAfterUIDestroyed();
  }
}

void SessionStateControllerImpl2::SetLockScreenDisplayedCallback(
    base::Closure& callback) {
  lock_screen_displayed_callback_ = callback;
}

void SessionStateControllerImpl2::OnStartingLock() {
  if (shutting_down_ || system_is_locked_)
    return;
  if (animating_lock_)
    return;
  StartImmediatePreLockAnimation(false /* request_lock_on_completion */);
}

void SessionStateControllerImpl2::StartLockAnimationAndLockImmediately() {
  if (animating_lock_)
    return;
  StartImmediatePreLockAnimation(true /* request_lock_on_completion */);
}

void SessionStateControllerImpl2::StartLockAnimation(bool shutdown_after_lock) {
  if (animating_lock_)
    return;
  shutdown_after_lock_ = shutdown_after_lock;
  can_cancel_lock_animation_ = true;

  StartCancellablePreLockAnimation();
}

bool SessionStateControllerImpl2::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool SessionStateControllerImpl2::ShutdownRequested() {
  return shutting_down_;
}

bool SessionStateControllerImpl2::CanCancelLockAnimation() {
  return can_cancel_lock_animation_;
}

void SessionStateControllerImpl2::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animating_lock_ = false;
  CancelPreLockAnimation();
}

bool SessionStateControllerImpl2::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         shutdown_after_lock_ ||
         lock_to_shutdown_timer_.IsRunning();
}

void SessionStateControllerImpl2::StartShutdownAnimation() {
  StartCancellableShutdownAnimation();
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
      internal::SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
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
  shell->cursor_manager()->HideCursor();

  StartShutdownAnimationImpl();
}

void SessionStateControllerImpl2::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  Shell::GetInstance()->delegate()->Exit();
}

void SessionStateControllerImpl2::OnLockFailTimeout() {
  DCHECK(!system_is_locked_);
  // Undo lock animation.
  StartUnlockAnimationAfterUIDestroyed();
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
  StartCancellableShutdownAnimation();
}

void SessionStateControllerImpl2::StartCancellableShutdownAnimation() {
  animator_->StartGlobalAnimation(
      internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartPreShutdownAnimationTimer();
}

void SessionStateControllerImpl2::StartShutdownAnimationImpl() {
  animator_->StartGlobalAnimation(
      internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartRealShutdownTimer(true);
}

void SessionStateControllerImpl2::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      animator_->
          GetDuration(internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN),
      this,
      &SessionStateControllerImpl2::OnPreShutdownAnimationTimeout);
}

void SessionStateControllerImpl2::OnPreShutdownAnimationTimeout() {
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->env_filter()->set_cursor_hidden_by_filter(false);
  shell->cursor_manager()->HideCursor();

  StartRealShutdownTimer(false);
}

void SessionStateControllerImpl2::StartRealShutdownTimer(
    bool with_animation_time) {
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kShutdownRequestDelayMs);
  if (with_animation_time) {
    duration += animator_->GetDuration(
        internal::SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  }
  real_shutdown_timer_.Start(
      FROM_HERE,
      duration,
      this,
      &SessionStateControllerImpl2::OnRealShutdownTimeout);
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
  StartUnlockAnimationBeforeUIDestroyed(callback);
}

void SessionStateControllerImpl2::LockAnimationCancelled() {
  can_cancel_lock_animation_ = false;
  RestoreUnlockedProperties();
}

void SessionStateControllerImpl2::PreLockAnimationFinished(bool request_lock) {
  can_cancel_lock_animation_ = false;

  if (request_lock)
    delegate_->RequestLockScreen();

  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this,
      &SessionStateControllerImpl2::OnLockFailTimeout);
}

void SessionStateControllerImpl2::PostLockAnimationFinished() {
  animating_lock_ = false;

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

void SessionStateControllerImpl2::UnlockAnimationAfterUIDestroyedFinished() {
  RestoreUnlockedProperties();
}

void SessionStateControllerImpl2::StartImmediatePreLockAnimation(
    bool request_lock_on_completion) {
  animating_lock_ = true;

  StoreUnlockedProperties();

  base::Closure next_animation_starter =
      base::Bind(&SessionStateControllerImpl2::PreLockAnimationFinished,
      base::Unretained(this), request_lock_on_completion);
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_FADE_OUT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateBackgroundAppearanceIfNecessary(
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);

  observer->Unpause();

  DispatchCancelMode();
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(SessionStateObserver::EVENT_LOCK_ANIMATION_STARTED));
}

void SessionStateControllerImpl2::StartCancellablePreLockAnimation() {
  animating_lock_ = true;
  StoreUnlockedProperties();

  base::Closure next_animation_starter =
      base::Bind(&SessionStateControllerImpl2::PreLockAnimationFinished,
      base::Unretained(this), true /* request_lock */);
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE,
      observer);
  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_FADE_OUT,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE,
      observer);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
      internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateBackgroundAppearanceIfNecessary(
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDOABLE,
      observer);

  DispatchCancelMode();
  FOR_EACH_OBSERVER(SessionStateObserver, observers_,
      OnSessionStateEvent(
          SessionStateObserver::EVENT_PRELOCK_ANIMATION_STARTED));
  observer->Unpause();
}

void SessionStateControllerImpl2::CancelPreLockAnimation() {
  base::Closure next_animation_starter =
      base::Bind(&SessionStateControllerImpl2::LockAnimationCancelled,
      base::Unretained(this));
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_UNDO_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_FADE_IN,
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      observer);
  AnimateBackgroundHidingIfNecessary(
      internal::SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      observer);

  observer->Unpause();
}

void SessionStateControllerImpl2::StartPostLockAnimation() {
  base::Closure next_animation_starter =
      base::Bind(&SessionStateControllerImpl2::PostLockAnimationFinished,
      base::Unretained(this));

  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();
  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  observer->Unpause();
}

void SessionStateControllerImpl2::StartUnlockAnimationBeforeUIDestroyed(
    base::Closure& callback) {
  animator_->StartAnimationWithCallback(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_LIFT,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      callback);
}

void SessionStateControllerImpl2::StartUnlockAnimationAfterUIDestroyed() {
  base::Closure next_animation_starter =
      base::Bind(
          &SessionStateControllerImpl2::UnlockAnimationAfterUIDestroyedFinished,
      base::Unretained(this));

  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_DROP,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_FADE_IN,
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  AnimateBackgroundHidingIfNecessary(
      internal::SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  observer->Unpause();
}

void SessionStateControllerImpl2::StoreUnlockedProperties() {
  if (!unlocked_properties_.get()) {
    unlocked_properties_.reset(new UnlockedStateProperties());
    unlocked_properties_->background_is_hidden = IsBackgroundHidden();
  }
  if (unlocked_properties_->background_is_hidden) {
    // Hide background so that it can be animated later.
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    ShowBackground();
  }
}

void SessionStateControllerImpl2::RestoreUnlockedProperties() {
  if (!unlocked_properties_.get())
    return;
  if (unlocked_properties_->background_is_hidden) {
    HideBackground();
    // Restore background visibility.
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_FADE_IN,
        internal::SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
  unlocked_properties_.reset();
}

void SessionStateControllerImpl2::AnimateBackgroundAppearanceIfNecessary(
    internal::SessionStateAnimator::AnimationSpeed speed,
    ui::LayerAnimationObserver* observer) {
  if (unlocked_properties_.get() &&
      unlocked_properties_->background_is_hidden) {
    animator_->StartAnimationWithObserver(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_FADE_IN,
        speed,
        observer);
  }
}

void SessionStateControllerImpl2::AnimateBackgroundHidingIfNecessary(
    internal::SessionStateAnimator::AnimationSpeed speed,
    ui::LayerAnimationObserver* observer) {
  if (unlocked_properties_.get() &&
      unlocked_properties_->background_is_hidden) {
    animator_->StartAnimationWithObserver(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_FADE_OUT,
        speed,
        observer);
  }
}

}  // namespace ash
