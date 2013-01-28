// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_CONTROLLER_IMPL2_H_
#define ASH_WM_SESSION_STATE_CONTROLLER_IMPL2_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_controller.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/aura/root_window_observer.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Layer;
}

namespace ash {

namespace test {
class SessionStateControllerImpl2Test;
}

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
// Lock workflow:
// Entry points:
//  * StartLockAnimation (bool shutdown after lock) - starts lock that can be
//    cancelled.
//  * StartLockAnimationAndLockImmediately - starts uninterruptible lock
//    animation.
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

class ASH_EXPORT SessionStateControllerImpl2 : public SessionStateController {
 public:

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateControllerImpl2* controller);

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
    SessionStateControllerImpl2* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  SessionStateControllerImpl2();
  virtual ~SessionStateControllerImpl2();

  // RootWindowObserver override:
  virtual void OnRootWindowHostCloseRequested(
     const aura::RootWindow* root) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

  // SessionStateController overrides:
  virtual void StartLockAnimation(bool shutdown_after_lock) OVERRIDE;

  virtual void StartShutdownAnimation() OVERRIDE;
  virtual void StartLockAnimationAndLockImmediately() OVERRIDE;

  virtual bool LockRequested() OVERRIDE;
  virtual bool ShutdownRequested() OVERRIDE;

  virtual bool CanCancelLockAnimation() OVERRIDE;
  virtual void CancelLockAnimation() OVERRIDE;

  virtual bool CanCancelShutdownAnimation() OVERRIDE;
  virtual void CancelShutdownAnimation() OVERRIDE;

  virtual void OnStartingLock() OVERRIDE;
  virtual void RequestShutdown() OVERRIDE;

  virtual void OnLockScreenHide(base::Closure& callback) OVERRIDE;
  virtual void SetLockScreenDisplayedCallback(base::Closure& callback) OVERRIDE;

 protected:
  friend class test::SessionStateControllerImpl2Test;

 private:
  struct UnlockedStateProperties {
    bool background_is_hidden;
  };

  void RequestShutdownImpl();

  // Reverts the pre-lock animation, reports the error.
  void OnLockFailTimeout();

  // Starts timer for gap between lock and shutdown.
  void StartLockToShutdownTimer();

  // Calls StartShutdownAnimation().
  void OnLockToShutdownTimeout();

  // Starts timer for undoable shutdown animation.
  void StartPreShutdownAnimationTimer();

  // Calls RequestShutdownImpl();
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

  // Starts non-cancellable animation and starts real shutdown timer that
  // includes animation time.
  void StartShutdownAnimationImpl();

  // Triggers late animations on the lock screen.
  void OnLockScreenAnimationFinished();

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
      ash::internal::SessionStateAnimator::AnimationSpeed speed,
      ui::LayerAnimationObserver* observer);

  // Fades out background layer with |speed| if it was hidden in unlocked state.
  void AnimateBackgroundHidingIfNecessary(
      ash::internal::SessionStateAnimator::AnimationSpeed speed,
      ui::LayerAnimationObserver* observer);

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
  base::OneShotTimer<SessionStateControllerImpl2> lock_fail_timer_;

  // Started when the screen is locked while the power button is held.  Adds a
  // delay between the appearance of the lock screen and the beginning of the
  // pre-shutdown animation.
  base::OneShotTimer<SessionStateControllerImpl2> lock_to_shutdown_timer_;

  // Started when we begin displaying the pre-shutdown animation.  When it
  // fires, we start the shutdown animation and get ready to request shutdown.
  base::OneShotTimer<SessionStateControllerImpl2> pre_shutdown_timer_;

  // Started when we display the shutdown animation.  When it fires, we actually
  // request shutdown.  Gives the animation time to complete before Chrome, X,
  // etc. are shut down.
  base::OneShotTimer<SessionStateControllerImpl2> real_shutdown_timer_;

  base::Closure lock_screen_displayed_callback_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerImpl2);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_CONTROLLER_IMPL2_H_
