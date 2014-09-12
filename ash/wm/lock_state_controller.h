// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_STATE_CONTROLLER_H_
#define ASH_WM_LOCK_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/lock_state_observer.h"
#include "ash/wm/session_state_animator.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tree_host_observer.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Layer;
}

namespace ash {

namespace test {
class LockStateControllerTest;
class PowerButtonControllerTest;
}

// Performs system-related functions on behalf of LockStateController.
class ASH_EXPORT LockStateControllerDelegate {
 public:
  LockStateControllerDelegate() {}
  virtual ~LockStateControllerDelegate() {}

  virtual void RequestLockScreen() = 0;
  virtual void RequestShutdown() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockStateControllerDelegate);
};

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
// Lock workflow:
// Entry points:
//  * StartLockAnimation (bool shutdown after lock) - starts lock that can be
//    cancelled.
//  * StartLockAnimationAndLockImmediately (bool shutdown after lock) - starts
//    uninterruptible lock animation.
// This leads to call of either StartImmediatePreLockAnimation or
// StartCancellablePreLockAnimation. Once they complete
// PreLockAnimationFinished is called, and system lock is requested.
// Once system locks and lock UI is created, OnLockStateChanged is called, and
// StartPostLockAnimation is called. In PostLockAnimationFinished two
// things happen : EVENT_LOCK_ANIMATION_FINISHED notification is sent (it
// triggers third part of animation within lock UI), and check for continuing to
// shutdown is made.
//
// Unlock workflow:
// WebUI does first part of animation, and calls OnLockScreenHide(callback) that
// triggers StartUnlockAnimationBeforeUIDestroyed(callback). Once callback is
// called at the end of the animation, lock UI is deleted, system unlocks, and
// OnLockStateChanged is called. It leads to
// StartUnlockAnimationAfterUIDestroyed.
class ASH_EXPORT LockStateController : public aura::WindowTreeHostObserver,
                                       public ShellObserver {
 public:
  // Amount of time that the power button needs to be held before we lock the
  // screen.
  static const int kLockTimeoutMs;

  // Amount of time that the power button needs to be held before we shut down.
  static const int kShutdownTimeoutMs;

  // Amount of time to wait for our lock requests to be honored before giving
  // up.
  static const int kLockFailTimeoutMs;

  // When the button has been held continuously from the unlocked state, amount
  // of time that we wait after the screen locker window is shown before
  // starting the pre-shutdown animation.
  static const int kLockToShutdownTimeoutMs;

  // Additional time (beyond kFastCloseAnimMs) to wait after starting the
  // fast-close shutdown animation before actually requesting shutdown, to give
  // the animation time to finish.
  static const int kShutdownRequestDelayMs;

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LockStateController* controller);

    virtual ~TestApi();

    bool lock_fail_timer_is_running() const {
      return controller_->lock_fail_timer_.IsRunning();
    }
    bool lock_to_shutdown_timer_is_running() const {
      return controller_->lock_to_shutdown_timer_.IsRunning();
    }
    bool shutdown_timer_is_running() const {
      return controller_->pre_shutdown_timer_.IsRunning();
    }
    bool real_shutdown_timer_is_running() const {
      return controller_->real_shutdown_timer_.IsRunning();
    }
    bool is_animating_lock() const {
      return controller_->animating_lock_;
    }
    bool is_lock_cancellable() const {
      return controller_->CanCancelLockAnimation();
    }

    void trigger_lock_fail_timeout() {
      controller_->OnLockFailTimeout();
      controller_->lock_fail_timer_.Stop();
    }
    void trigger_lock_to_shutdown_timeout() {
      controller_->OnLockToShutdownTimeout();
      controller_->lock_to_shutdown_timer_.Stop();
    }
    void trigger_shutdown_timeout() {
      controller_->OnPreShutdownAnimationTimeout();
      controller_->pre_shutdown_timer_.Stop();
    }
    void trigger_real_shutdown_timeout() {
      controller_->OnRealShutdownTimeout();
      controller_->real_shutdown_timer_.Stop();
    }

   private:
    LockStateController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  LockStateController();
  virtual ~LockStateController();

  void SetDelegate(scoped_ptr<LockStateControllerDelegate> delegate);

  void AddObserver(LockStateObserver* observer);
  void RemoveObserver(LockStateObserver* observer);
  bool HasObserver(LockStateObserver* observer);

  // Starts locking (with slow animation) that can be cancelled.
  // After locking and |kLockToShutdownTimeoutMs| StartShutdownAnimation()
  // will be called unless CancelShutdownAnimation() is called, if
  // |shutdown_after_lock| is true.
  void StartLockAnimation(bool shutdown_after_lock);

  // Starts shutting down (with slow animation) that can be cancelled.
  void StartShutdownAnimation();

  // Starts usual lock animation, but locks immediately.  After locking and
  // |kLockToShutdownTimeoutMs| StartShutdownAnimation() will be called unless
  // CancelShutdownAnimation() is called, if  |shutdown_after_lock| is true.
  void StartLockAnimationAndLockImmediately(bool shutdown_after_lock);

  // Returns true if we have requested system to lock, but haven't received
  // confirmation yet.
  bool LockRequested();

  // Returns true if we are shutting down.
  bool ShutdownRequested();

  // Returns true if we are within cancellable lock timeframe.
  bool CanCancelLockAnimation();

  // Cancels locking and reverts lock animation.
  void CancelLockAnimation();

  // Returns true if we are within cancellable shutdown timeframe.
  bool CanCancelShutdownAnimation();

  // Cancels shutting down and reverts shutdown animation.
  void CancelShutdownAnimation();

  // Called when Chrome gets a request to display the lock screen.
  void OnStartingLock();

  // Displays the shutdown animation and requests shutdown when it's done.
  void RequestShutdown();

  // Called when ScreenLocker is ready to close, but not yet destroyed.
  // Can be used to display "hiding" animations on unlock.
  // |callback| will be called when all animations are done.
  void OnLockScreenHide(base::Closure& callback);

  // Sets up the callback that should be called once lock animation is finished.
  // Callback is guaranteed to be called once and then discarded.
  void SetLockScreenDisplayedCallback(const base::Closure& callback);

  // aura::WindowTreeHostObserver override:
  virtual void OnHostCloseRequested(const aura::WindowTreeHost* host) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

  void set_animator_for_test(SessionStateAnimator* animator) {
    animator_.reset(animator);
  }

 private:
  friend class test::PowerButtonControllerTest;
  friend class test::LockStateControllerTest;

  struct UnlockedStateProperties {
    bool background_is_hidden;
  };

  // Reverts the pre-lock animation, reports the error.
  void OnLockFailTimeout();

  // Starts timer for gap between lock and shutdown.
  void StartLockToShutdownTimer();

  // Calls StartShutdownAnimation().
  void OnLockToShutdownTimeout();

  // Starts timer for undoable shutdown animation.
  void StartPreShutdownAnimationTimer();

  // Calls StartRealShutdownTimer().
  void OnPreShutdownAnimationTimeout();

  // Starts timer for final shutdown animation.
  // If |with_animation_time| is true, it will also include time of "fade to
  // white" shutdown animation.
  void StartRealShutdownTimer(bool with_animation_time);

  // Requests that the machine be shut down.
  void OnRealShutdownTimeout();

  // Starts shutdown animation that can be cancelled and starts pre-shutdown
  // timer.
  void StartCancellableShutdownAnimation();

  // If |request_lock_on_completion| is true, a lock request will be sent
  // after the pre-lock animation completes.  (The pre-lock animation is
  // also displayed in response to already-in-progress lock requests; in
  // these cases an additional lock request is undesirable.)
  void StartImmediatePreLockAnimation(bool request_lock_on_completion);
  void StartCancellablePreLockAnimation();
  void CancelPreLockAnimation();
  void StartPostLockAnimation();
  // This method calls |callback| when animation completes.
  void StartUnlockAnimationBeforeUIDestroyed(base::Closure &callback);
  void StartUnlockAnimationAfterUIDestroyed();

  // These methods are called when corresponding animation completes.
  void LockAnimationCancelled();
  void PreLockAnimationFinished(bool request_lock);
  void PostLockAnimationFinished();
  void UnlockAnimationAfterUIDestroyedFinished();

  // Stores properties of UI that have to be temporarily modified while locking.
  void StoreUnlockedProperties();
  void RestoreUnlockedProperties();

  // Fades in background layer with |speed| if it was hidden in unlocked state.
  void AnimateBackgroundAppearanceIfNecessary(
      ash::SessionStateAnimator::AnimationSpeed speed,
      SessionStateAnimator::AnimationSequence* animation_sequence);

  // Fades out background layer with |speed| if it was hidden in unlocked state.
  void AnimateBackgroundHidingIfNecessary(
      ash::SessionStateAnimator::AnimationSpeed speed,
      SessionStateAnimator::AnimationSequence* animation_sequence);

  scoped_ptr<SessionStateAnimator> animator_;

  scoped_ptr<LockStateControllerDelegate> delegate_;

  ObserverList<LockStateObserver> observers_;

  // The current login status, or original login status from before we locked.
  user::LoginStatus login_status_;

  // Current lock status.
  bool system_is_locked_;

  // Are we in the process of shutting the machine down?
  bool shutting_down_;

  // Indicates whether controller should proceed to (cancellable) shutdown after
  // locking.
  bool shutdown_after_lock_;

  // Indicates that controller displays lock animation.
  bool animating_lock_;

  // Indicates that lock animation can be undone.
  bool can_cancel_lock_animation_;

  scoped_ptr<UnlockedStateProperties> unlocked_properties_;

  // Started when we request that the screen be locked.  When it fires, we
  // assume that our request got dropped.
  base::OneShotTimer<LockStateController> lock_fail_timer_;

  // Started when the screen is locked while the power button is held.  Adds a
  // delay between the appearance of the lock screen and the beginning of the
  // pre-shutdown animation.
  base::OneShotTimer<LockStateController> lock_to_shutdown_timer_;

  // Started when we begin displaying the pre-shutdown animation.  When it
  // fires, we start the shutdown animation and get ready to request shutdown.
  base::OneShotTimer<LockStateController> pre_shutdown_timer_;

  // Started when we display the shutdown animation.  When it fires, we actually
  // request shutdown.  Gives the animation time to complete before Chrome, X,
  // etc. are shut down.
  base::OneShotTimer<LockStateController> real_shutdown_timer_;

  base::Closure lock_screen_displayed_callback_;

  base::WeakPtrFactory<LockStateController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LockStateController);
};

}  // namespace ash

#endif  // ASH_WM_LOCK_STATE_CONTROLLER_H_
