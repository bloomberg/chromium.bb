// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/cancel_mode.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/shutdown.mojom.h"
#include "ash/shell.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_animator_impl.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/wm/core/compound_event_filter.h"

#define UMA_HISTOGRAM_LOCK_TIMES(name, sample)                     \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                         \
                             base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromSeconds(50), 100)

namespace ash {

namespace {

const int kMaxShutdownSoundDurationMs = 1500;

}  // namespace

// ASan/TSan/MSan instrument each memory access. This may slow the execution
// down significantly.
#if defined(MEMORY_SANITIZER)
// For MSan the slowdown depends heavily on the value of msan_track_origins GYP
// flag. The multiplier below corresponds to msan_track_origins=1.
static const int kTimeoutMultiplier = 6;
#elif defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(SYZYASAN)
static const int kTimeoutMultiplier = 2;
#else
static const int kTimeoutMultiplier = 1;
#endif

const int LockStateController::kLockFailTimeoutMs = 8000 * kTimeoutMultiplier;
const int LockStateController::kLockToShutdownTimeoutMs = 150;
const int LockStateController::kShutdownRequestDelayMs = 50;

LockStateController::LockStateController(
    ShutdownController* shutdown_controller)
    : animator_(new SessionStateAnimatorImpl()),
      login_status_(LoginStatus::NOT_LOGGED_IN),
      system_is_locked_(false),
      shutting_down_(false),
      shutdown_after_lock_(false),
      animating_lock_(false),
      can_cancel_lock_animation_(false),
      shutdown_controller_(shutdown_controller),
      weak_ptr_factory_(this) {
  DCHECK(shutdown_controller_);
  Shell::GetPrimaryRootWindow()->GetHost()->AddObserver(this);
}

LockStateController::~LockStateController() {
  Shell::GetPrimaryRootWindow()->GetHost()->RemoveObserver(this);
}

void LockStateController::StartLockAnimation(bool shutdown_after_lock) {
  if (animating_lock_)
    return;
  shutdown_after_lock_ = shutdown_after_lock;
  can_cancel_lock_animation_ = true;

  StartCancellablePreLockAnimation();
}

void LockStateController::StartShutdownAnimation() {
  StartCancellableShutdownAnimation();
}

void LockStateController::StartLockAnimationAndLockImmediately(
    bool shutdown_after_lock) {
  if (animating_lock_)
    return;
  shutdown_after_lock_ = shutdown_after_lock;
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
  return pre_shutdown_timer_.IsRunning() || shutdown_after_lock_ ||
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

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
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

  Shell* shell = Shell::GetInstance();
  // TODO(derat): Remove these null checks once mash instantiates a
  // CursorManager.
  if (shell->cursor_manager()) {
    shell->cursor_manager()->HideCursor();
    shell->cursor_manager()->LockCursor();
  }

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
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
  WmShell::Get()->delegate()->Exit();
}

void LockStateController::OnLoginStateChanged(LoginStatus status) {
  if (status != LoginStatus::LOCKED)
    login_status_ = status;
  system_is_locked_ = (status == LoginStatus::LOCKED);
}

void LockStateController::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  // This is also the case when the user signs off.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = Shell::GetInstance();
    if (shell->cursor_manager()) {
      shell->cursor_manager()->HideCursor();
      shell->cursor_manager()->LockCursor();
    }
    animator_->StartAnimation(SessionStateAnimator::kAllNonRootContainersMask,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
}

void LockStateController::OnLockStateChanged(bool locked) {
  DCHECK((lock_fail_timer_.IsRunning() && lock_duration_timer_ != nullptr) ||
         (!lock_fail_timer_.IsRunning() && lock_duration_timer_ == nullptr));
  VLOG(1) << "OnLockStateChanged called with locked: " << locked
          << ", shutting_down_: " << shutting_down_
          << ", system_is_locked_: " << system_is_locked_
          << ", lock_fail_timer_.IsRunning(): " << lock_fail_timer_.IsRunning();

  if (shutting_down_ || (system_is_locked_ == locked))
    return;

  system_is_locked_ = locked;

  if (locked) {
    StartPostLockAnimation();

    lock_fail_timer_.Stop();

    if (lock_duration_timer_) {
      UMA_HISTOGRAM_LOCK_TIMES("Ash.WindowManager.Lock.Success",
                               lock_duration_timer_->Elapsed());
      lock_duration_timer_.reset();
    }
  } else {
    StartUnlockAnimationAfterUIDestroyed();
  }
}

void LockStateController::OnLockFailTimeout() {
  UMA_HISTOGRAM_LOCK_TIMES("Ash.WindowManager.Lock.Timeout",
                           lock_duration_timer_->Elapsed());
  lock_duration_timer_.reset();
  DCHECK(!system_is_locked_);

  LOG(FATAL) << "Screen lock took too long; crashing intentionally";
}

void LockStateController::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
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
      this, &LockStateController::OnPreShutdownAnimationTimeout);
}

void LockStateController::OnPreShutdownAnimationTimeout() {
  VLOG(1) << "OnPreShutdownAnimationTimeout";
  shutting_down_ = true;

  Shell* shell = Shell::GetInstance();
  if (shell->cursor_manager())
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

  base::TimeDelta sound_duration =
      WmShell::Get()->accessibility_delegate()->PlayShutdownSound();
  sound_duration =
      std::min(sound_duration,
               base::TimeDelta::FromMilliseconds(kMaxShutdownSoundDurationMs));
  duration = std::max(duration, sound_duration);

  real_shutdown_timer_.Start(
      FROM_HERE, duration, base::Bind(&LockStateController::OnRealPowerTimeout,
                                      base::Unretained(this)));
}

void LockStateController::OnRealPowerTimeout() {
  VLOG(1) << "OnRealPowerTimeout";
  DCHECK(shutting_down_);
  WmShell::Get()->RecordUserMetricsAction(UMA_ACCEL_SHUT_DOWN_POWER_BUTTON);
  // Shut down or reboot based on device policy.
  shutdown_controller_->ShutDownOrReboot();
}

void LockStateController::StartCancellableShutdownAnimation() {
  Shell* shell = Shell::GetInstance();
  // Hide cursor, but let it reappear if the mouse moves.
  if (shell->cursor_manager())
    shell->cursor_manager()->HideCursor();

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
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
                 weak_ptr_factory_.GetWeakPtr(), request_lock_on_completion);
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(next_animation_starter);

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->StartAnimation(
      SessionStateAnimator::LAUNCHER, SessionStateAnimator::ANIMATION_FADE_OUT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateWallpaperAppearanceIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, animation_sequence);

  animation_sequence->EndSequence();

  DispatchCancelMode();
  WmShell::Get()->OnLockStateEvent(
      LockStateObserver::EVENT_LOCK_ANIMATION_STARTED);
}

void LockStateController::StartCancellablePreLockAnimation() {
  animating_lock_ = true;
  StoreUnlockedProperties();
  VLOG(1) << "StartCancellablePreLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::PreLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr(), true /* request_lock */);
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(next_animation_starter);

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  animation_sequence->StartAnimation(
      SessionStateAnimator::LAUNCHER, SessionStateAnimator::ANIMATION_FADE_OUT,
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateWallpaperAppearanceIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, animation_sequence);

  DispatchCancelMode();
  WmShell::Get()->OnLockStateEvent(
      LockStateObserver::EVENT_PRELOCK_ANIMATION_STARTED);
  animation_sequence->EndSequence();
}

void LockStateController::CancelPreLockAnimation() {
  VLOG(1) << "CancelPreLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::LockAnimationCancelled,
                 weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(next_animation_starter);

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_UNDO_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);
  animation_sequence->StartAnimation(
      SessionStateAnimator::LAUNCHER, SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);
  AnimateWallpaperHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      animation_sequence);

  animation_sequence->EndSequence();
}

void LockStateController::StartPostLockAnimation() {
  VLOG(1) << "StartPostLockAnimation";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::PostLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(next_animation_starter);

  animation_sequence->StartAnimation(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->EndSequence();
}

void LockStateController::StartUnlockAnimationBeforeUIDestroyed(
    base::Closure& callback) {
  VLOG(1) << "StartUnlockAnimationBeforeUIDestroyed";
  animator_->StartAnimationWithCallback(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, callback);
}

void LockStateController::StartUnlockAnimationAfterUIDestroyed() {
  VLOG(1) << "StartUnlockAnimationAfterUIDestroyed";
  base::Closure next_animation_starter =
      base::Bind(&LockStateController::UnlockAnimationAfterUIDestroyedFinished,
                 weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(next_animation_starter);

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_DROP,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->StartAnimation(
      SessionStateAnimator::LAUNCHER, SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  AnimateWallpaperHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, animation_sequence);
  animation_sequence->EndSequence();
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
    WmShell::Get()->RecordUserMetricsAction(
        shutdown_after_lock_ ? UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON
                             : UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON);
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->RequestLockScreen();
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs);
  // Increase lock timeout for slower hardware, see http://crbug.com/350628
  // The devices with boards "x86-mario", "daisy", "x86-alex" and "x86-zgb" have
  // slower hardware. For "x86-alex" and "x86-zgb" there are some modifications
  // like "x86-alex-he". Also there's "daisy", "daisy_spring" and "daisy_skate",
  // but they are all different devices and only "daisy" has slower hardware.
  const std::string board = base::SysInfo::GetStrippedReleaseBoard();
  if (board == "x86-mario" || board == "daisy" ||
      base::StartsWith(board, "x86-alex", base::CompareCase::SENSITIVE) ||
      base::StartsWith(board, "x86-zgb", base::CompareCase::SENSITIVE)) {
    timeout *= 2;
  }
  lock_fail_timer_.Start(FROM_HERE, timeout, this,
                         &LockStateController::OnLockFailTimeout);

  lock_duration_timer_.reset(new base::ElapsedTimer());
}

void LockStateController::PostLockAnimationFinished() {
  animating_lock_ = false;
  VLOG(1) << "PostLockAnimationFinished";
  WmShell::Get()->OnLockStateEvent(
      LockStateObserver::EVENT_LOCK_ANIMATION_FINISHED);
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
    unlocked_properties_->wallpaper_is_hidden = animator_->IsWallpaperHidden();
  }
  if (unlocked_properties_->wallpaper_is_hidden) {
    // Hide wallpaper so that it can be animated later.
    animator_->StartAnimation(SessionStateAnimator::WALLPAPER,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    animator_->ShowWallpaper();
  }
}

void LockStateController::RestoreUnlockedProperties() {
  if (!unlocked_properties_)
    return;
  if (unlocked_properties_->wallpaper_is_hidden) {
    animator_->HideWallpaper();
    // Restore wallpaper visibility.
    animator_->StartAnimation(SessionStateAnimator::WALLPAPER,
                              SessionStateAnimator::ANIMATION_FADE_IN,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
  unlocked_properties_.reset();
}

void LockStateController::AnimateWallpaperAppearanceIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    SessionStateAnimator::AnimationSequence* animation_sequence) {
  if (unlocked_properties_.get() && unlocked_properties_->wallpaper_is_hidden) {
    animation_sequence->StartAnimation(SessionStateAnimator::WALLPAPER,
                                       SessionStateAnimator::ANIMATION_FADE_IN,
                                       speed);
  }
}

void LockStateController::AnimateWallpaperHidingIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    SessionStateAnimator::AnimationSequence* animation_sequence) {
  if (unlocked_properties_.get() && unlocked_properties_->wallpaper_is_hidden) {
    animation_sequence->StartAnimation(SessionStateAnimator::WALLPAPER,
                                       SessionStateAnimator::ANIMATION_FADE_OUT,
                                       speed);
  }
}

}  // namespace ash
