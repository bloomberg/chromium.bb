// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <algorithm>

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/cancel_mode.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/wm/core/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "media/audio/sounds/sounds_manager.h"
#endif

#if defined(OS_CHROMEOS)
using media::SoundsManager;
#endif

namespace ash {

namespace {

#if defined(OS_CHROMEOS)
const int kMaxShutdownSoundDurationMs = 1500;
#endif

aura::Window* GetBackground() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return Shell::GetContainer(root_window,
                             kShellWindowId_DesktopBackgroundContainer);
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

}  // namespace

const int LockStateController::kLockTimeoutMs = 400;
const int LockStateController::kShutdownTimeoutMs = 400;
const int LockStateController::kLockFailTimeoutMs = 8000;
const int LockStateController::kLockToShutdownTimeoutMs = 150;
const int LockStateController::kShutdownRequestDelayMs = 50;

LockStateController::TestApi::TestApi(LockStateController* controller)
    : controller_(controller) {
}

LockStateController::TestApi::~TestApi() {
}

LockStateController::LockStateController()
    : animator_(new SessionStateAnimator()),
      login_status_(user::LOGGED_IN_NONE),
      system_is_locked_(false),
      shutting_down_(false),
      shutdown_after_lock_(false),
      animating_lock_(false),
      can_cancel_lock_animation_(false),
      weak_ptr_factory_(this) {
  Shell::GetPrimaryRootWindow()->GetHost()->AddObserver(this);
}

LockStateController::~LockStateController() {
  Shell::GetPrimaryRootWindow()->GetHost()->RemoveObserver(this);
}

void LockStateController::SetDelegate(LockStateControllerDelegate* delegate) {
  delegate_.reset(delegate);
}

void LockStateController::AddObserver(LockStateObserver* observer) {
  observers_.AddObserver(observer);
}

void LockStateController::RemoveObserver(LockStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool LockStateController::HasObserver(LockStateObserver* observer) {
  return observers_.HasObserver(observer);
}

void LockStateController::StartLockAnimation(
    bool shutdown_after_lock) {
  if (animating_lock_)
    return;
  shutdown_after_lock_ = shutdown_after_lock;
  can_cancel_lock_animation_ = true;

  StartCancellablePreLockAnimation();
}

void LockStateController::StartShutdownAnimation() {
  StartCancellableShutdownAnimation();
}

void LockStateController::StartLockAnimationAndLockImmediately() {
  if (animating_lock_)
    return;
  StartImmediatePreLockAnimation(true /* request_lock_on_completion */);
}

bool LockStateController::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool LockStateController::ShutdownRequested() {
  return shutting_down_;
}

bool LockStateController::CanCancelLockAnimation() {
  return can_cancel_lock_animation_;
}

void LockStateController::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animating_lock_ = false;
  CancelPreLockAnimation();
}

bool LockStateController::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         shutdown_after_lock_ ||
         lock_to_shutdown_timer_.IsRunning();
}

void LockStateController::CancelShutdownAnimation() {
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
      SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
  pre_shutdown_timer_.Stop();
}

void LockStateController::OnStartingLock() {
  if (shutting_down_ || system_is_locked_)
    return;
  if (animating_lock_)
    return;
  StartImmediatePreLockAnimation(false /* request_lock_on_completion */);
}

void LockStateController::RequestShutdown() {
  if (shutting_down_)
    return;

  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->cursor_manager()->HideCursor();
  shell->cursor_manager()->LockCursor();

  animator_->StartGlobalAnimation(
      SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartRealShutdownTimer(true);
}

void LockStateController::OnLockScreenHide(
  base::Callback<void(void)>& callback) {
  StartUnlockAnimationBeforeUIDestroyed(callback);
}

void LockStateController::SetLockScreenDisplayedCallback(
    const base::Closure& callback) {
  lock_screen_displayed_callback_ = callback;
}

void LockStateController::OnHostCloseRequested(
    const aura::WindowTreeHost* host) {
  Shell::GetInstance()->delegate()->Exit();
}

void LockStateController::OnLoginStateChanged(
    user::LoginStatus status) {
  if (status != user::LOGGED_IN_LOCKED)
    login_status_ = status;
  system_is_locked_ = (status == user::LOGGED_IN_LOCKED);
}

void LockStateController::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  // This is also the case when the user signs off.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = ash::Shell::GetInstance();
    shell->cursor_manager()->HideCursor();
    shell->cursor_manager()->LockCursor();
    animator_->StartAnimation(SessionStateAnimator::kAllContainersMask,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
}

void LockStateController::OnLockStateChanged(bool locked) {
  VLOG(1) << "OnLockStateChanged " << locked;
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

void LockStateController::OnLockFailTimeout() {
  DCHECK(!system_is_locked_);
  CHECK(false) << "We can not be sure about the lock state. Crash and let the "
               << "SessionManager end the session";
}

void LockStateController::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
      this, &LockStateController::OnLockToShutdownTimeout);
}

void LockStateController::OnLockToShutdownTimeout() {
  DCHECK(system_is_locked_);
  StartCancellableShutdownAnimation();
}

void LockStateController::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN),
      this,
      &LockStateController::OnPreShutdownAnimationTimeout);
}

void LockStateController::OnPreShutdownAnimationTimeout() {
  VLOG(1) << "OnPreShutdownAnimationTimeout";
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->cursor_manager()->HideCursor();

  StartRealShutdownTimer(false);
}

void LockStateController::StartRealShutdownTimer(bool with_animation_time) {
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kShutdownRequestDelayMs);
  if (with_animation_time) {
    duration +=
        animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  }

#if defined(OS_CHROMEOS)
  const AccessibilityDelegate* const delegate =
      Shell::GetInstance()->accessibility_delegate();
  base::TimeDelta sound_duration = delegate->PlayShutdownSound();
  sound_duration =
      std::min(sound_duration,
               base::TimeDelta::FromMilliseconds(kMaxShutdownSoundDurationMs));
  duration = std::max(duration, sound_duration);
#endif

  real_shutdown_timer_.Start(
      FROM_HERE, duration, this, &LockStateController::OnRealShutdownTimeout);
}

void LockStateController::OnRealShutdownTimeout() {
  VLOG(1) << "OnRealShutdownTimeout";
  DCHECK(shutting_down_);
#if defined(OS_CHROMEOS)
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    ShellDelegate* delegate = Shell::GetInstance()->delegate();
    if (delegate) {
      delegate->Exit();
      return;
    }
  }
#endif
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      UMA_ACCEL_SHUT_DOWN_POWER_BUTTON);
  delegate_->RequestShutdown();
}

void LockStateController::StartCancellableShutdownAnimation() {
  Shell* shell = ash::Shell::GetInstance();
  // Hide cursor, but let it reappear if the mouse moves.
  shell->cursor_manager()->HideCursor();

  animator_->StartGlobalAnimation(
      SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartPreShutdownAnimationTimer();
}

void LockStateController::StartImmediatePreLockAnimation(
    bool request_lock_on_completion) {
  VLOG(1) << "StartImmediatePreLockAnimation " << request_lock_on_completion;
  animating_lock_ = true;
  StoreUnlockedProperties();

  base::Closure next_animation_starter =
      base::Bind(&LockStateController::PreLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_lock_on_completion);
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      SessionStateAnimator::LAUNCHER,
      SessionStateAnimator::ANIMATION_FADE_OUT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateBackgroundAppearanceIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, observer);

  observer->Unpause();

  DispatchCancelMode();
  FOR_EACH_OBSERVER(LockStateObserver, observers_,
      OnLockStateEvent(LockStateObserver::EVENT_LOCK_ANIMATION_STARTED));
}

void LockStateController::StartCancellablePreLockAnimation() {
  animating_lock_ = true;
  StoreUnlockedProperties();
  VLOG(1) << "StartCancellablePreLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::PreLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 true /* request_lock */);
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE,
      observer);
  animator_->StartAnimationWithObserver(
      SessionStateAnimator::LAUNCHER,
      SessionStateAnimator::ANIMATION_FADE_OUT,
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE,
      observer);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateBackgroundAppearanceIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, observer);

  DispatchCancelMode();
  FOR_EACH_OBSERVER(LockStateObserver, observers_,
      OnLockStateEvent(LockStateObserver::EVENT_PRELOCK_ANIMATION_STARTED));
  observer->Unpause();
}

void LockStateController::CancelPreLockAnimation() {
  VLOG(1) << "CancelPreLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::LockAnimationCancelled,
                 weak_ptr_factory_.GetWeakPtr());
  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_UNDO_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      SessionStateAnimator::LAUNCHER,
      SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      observer);
  AnimateBackgroundHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS, observer);

  observer->Unpause();
}

void LockStateController::StartPostLockAnimation() {
  VLOG(1) << "StartPostLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::PostLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr());

  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();
  animator_->StartAnimationWithObserver(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  observer->Unpause();
}

void LockStateController::StartUnlockAnimationBeforeUIDestroyed(
    base::Closure& callback) {
  VLOG(1) << "StartUnlockAnimationBeforeUIDestroyed";
  animator_->StartAnimationWithCallback(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      callback);
}

void LockStateController::StartUnlockAnimationAfterUIDestroyed() {
  VLOG(1) << "StartUnlockAnimationAfterUIDestroyed";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::UnlockAnimationAfterUIDestroyedFinished,
                 weak_ptr_factory_.GetWeakPtr());

  AnimationFinishedObserver* observer =
      new AnimationFinishedObserver(next_animation_starter);

  observer->Pause();

  animator_->StartAnimationWithObserver(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_DROP,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  animator_->StartAnimationWithObserver(
      SessionStateAnimator::LAUNCHER,
      SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS,
      observer);
  AnimateBackgroundHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, observer);
  observer->Unpause();
}

void LockStateController::LockAnimationCancelled() {
  can_cancel_lock_animation_ = false;
  RestoreUnlockedProperties();
}

void LockStateController::PreLockAnimationFinished(bool request_lock) {
  VLOG(1) << "PreLockAnimationFinished";
  can_cancel_lock_animation_ = false;

  // Don't do anything (including starting the lock-fail timer) if the screen
  // was already locked while the animation was going.
  if (system_is_locked_) {
    DCHECK(!request_lock) << "Got request to lock already-locked system "
                          << "at completion of pre-lock animation";
    return;
  }

  if (request_lock) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        shutdown_after_lock_ ?
        UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON :
        UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON);
    delegate_->RequestLockScreen();
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs);
#if defined(OS_CHROMEOS)
  // Increase lock timeout for slower hardware, see http://crbug.com/350628
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (board == "x86-mario" ||
      StartsWithASCII(board, "x86-alex", true /* case_sensitive */) ||
      StartsWithASCII(board, "x86-zgb", true /* case_sensitive */)) {
    timeout *= 2;
  }
#endif
  lock_fail_timer_.Start(
      FROM_HERE, timeout, this, &LockStateController::OnLockFailTimeout);
}

void LockStateController::PostLockAnimationFinished() {
  animating_lock_ = false;
  VLOG(1) << "PostLockAnimationFinished";
  FOR_EACH_OBSERVER(LockStateObserver, observers_,
      OnLockStateEvent(LockStateObserver::EVENT_LOCK_ANIMATION_FINISHED));
  if (!lock_screen_displayed_callback_.is_null()) {
    lock_screen_displayed_callback_.Run();
    lock_screen_displayed_callback_.Reset();
  }
  CHECK(!views::MenuController::GetActiveInstance());
  if (shutdown_after_lock_) {
    shutdown_after_lock_ = false;
    StartLockToShutdownTimer();
  }
}

void LockStateController::UnlockAnimationAfterUIDestroyedFinished() {
  RestoreUnlockedProperties();
}

void LockStateController::StoreUnlockedProperties() {
  if (!unlocked_properties_) {
    unlocked_properties_.reset(new UnlockedStateProperties());
    unlocked_properties_->background_is_hidden = IsBackgroundHidden();
  }
  if (unlocked_properties_->background_is_hidden) {
    // Hide background so that it can be animated later.
    animator_->StartAnimation(SessionStateAnimator::DESKTOP_BACKGROUND,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    ShowBackground();
  }
}

void LockStateController::RestoreUnlockedProperties() {
  if (!unlocked_properties_)
    return;
  if (unlocked_properties_->background_is_hidden) {
    HideBackground();
    // Restore background visibility.
    animator_->StartAnimation(SessionStateAnimator::DESKTOP_BACKGROUND,
                              SessionStateAnimator::ANIMATION_FADE_IN,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
  unlocked_properties_.reset();
}

void LockStateController::AnimateBackgroundAppearanceIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    ui::LayerAnimationObserver* observer) {
  if (unlocked_properties_.get() &&
      unlocked_properties_->background_is_hidden) {
    animator_->StartAnimationWithObserver(
        SessionStateAnimator::DESKTOP_BACKGROUND,
        SessionStateAnimator::ANIMATION_FADE_IN,
        speed,
        observer);
  }
}

void LockStateController::AnimateBackgroundHidingIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    ui::LayerAnimationObserver* observer) {
  if (unlocked_properties_.get() &&
      unlocked_properties_->background_is_hidden) {
    animator_->StartAnimationWithObserver(
        SessionStateAnimator::DESKTOP_BACKGROUND,
        SessionStateAnimator::ANIMATION_FADE_OUT,
        speed,
        observer);
  }
}

}  // namespace ash
