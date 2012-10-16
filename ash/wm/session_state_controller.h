// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_CONTROLLER_H_
#define ASH_WM_SESSION_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/session_state_animator.h"
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
class PowerButtonControllerTest;
}

// Performs system-related functions on behalf of SessionStateController.
class ASH_EXPORT SessionStateControllerDelegate {
 public:
  SessionStateControllerDelegate() {}
  virtual ~SessionStateControllerDelegate() {}

  virtual void RequestLockScreen() = 0;
  virtual void RequestShutdown() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerDelegate);
};

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
class ASH_EXPORT SessionStateController : public aura::RootWindowObserver,
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

  // Amount of time taken to scale the snapshot of the screen down to a
  // slightly-smaller size once the user starts holding the power button.  Used
  // for both the pre-lock and pre-shutdown animations.
  static const int kSlowCloseAnimMs;

  // Amount of time taken to scale the snapshot of the screen back to its
  // original size when the button is released.
  static const int kUndoSlowCloseAnimMs;

  // Amount of time taken to scale the snapshot down to a point in the center of
  // the screen once the screen has been locked or we've been notified that the
  // system is shutting down.
  static const int kFastCloseAnimMs;

  // Additional time (beyond kFastCloseAnimMs) to wait after starting the
  // fast-close shutdown animation before actually requesting shutdown, to give
  // the animation time to finish.
  static const int kShutdownRequestDelayMs;

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateController* controller);

    virtual ~TestApi();

    bool lock_timer_is_running() const {
      return controller_->lock_timer_.IsRunning();
    }
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

    void trigger_lock_timeout() {
      controller_->OnLockTimeout();
      controller_->lock_timer_.Stop();
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
    SessionStateController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  SessionStateController();
  virtual ~SessionStateController();

  // RootWindowObserver override:
  virtual void OnRootWindowHostCloseRequested(
     const aura::RootWindow* root) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

  void SetDelegate(SessionStateControllerDelegate* delegate);

  // Returns true iff when we're in state when user session can be locked.
  virtual bool IsEligibleForLock();

  // Returns true if system is locked.
  virtual bool IsLocked();

  // Starts locking (with slow animation) that can be cancelled.
  // After locking and |kLockToShutdownTimeoutMs| StartShutdownAnimation()
  // will be called unless CancelShutdown() is called.
  virtual void StartLockAnimation();

  // Starts shutting down (with slow animation) that can be cancelled.
  virtual void StartShutdownAnimation();

  // Starts usual lock animation, but locks immediately.
  // Unlike StartLockAnimation it does no lead to StartShutdownAnimation.
  virtual void StartLockAnimationAndLockImmediately();

  // Returns true if we have requested system to lock, but haven't received
  // confirmation yet.
  virtual bool LockRequested();

  // Returns true if we are shutting down.
  virtual bool ShutdownRequested();

  // Returns true if we are within cancellable lock timeframe.
  virtual bool CanCancelLockAnimation();

  // Cancels locking and reverts lock animation.
  virtual void CancelLockAnimation();

  // Cancels locking and reverts lock animation with slightly different
  // parameters. Seems to be some bug, but refactoring should keep all bugs.
  // TODO(antrim): remove this, animations should actually be the same.
  virtual void CancelLockWithOtherAnimation();

  // Returns true if we are within cancellable shutdown timeframe.
  virtual bool CanCancelShutdownAnimation();

  // Cancels shutting down and reverts shutdown animation.
  virtual void CancelShutdownAnimation();

  // Called when Chrome gets a request to display the lock screen.
  virtual void OnStartingLock();

  // Displays the shutdown animation and requests shutdown when it's done.
  virtual void RequestShutdown();

 protected:
  friend class test::PowerButtonControllerTest;

  bool IsLoggedInAsNonGuest() const;

 private:
  void RequestShutdownImpl();

  // Starts lock timer.
  void StartLockTimer();

  // Requests that the screen be locked and starts |lock_fail_timer_|.
  void OnLockTimeout();

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
  void StartRealShutdownTimer();

  // Requests that the machine be shut down.
  void OnRealShutdownTimeout();

  // The current login status.
  user::LoginStatus login_status_;

  // Original login status from before we locked. LOGGED_IN_NONE if it's not
  // locked.
  user::LoginStatus unlocked_login_status_;

  // Are we in the process of shutting the machine down?
  bool shutting_down_;

  // Indicates whether controller should proceed to (cancellable) shutdown after
  // locking.
  bool shutdown_after_lock_;

  // Started when the user first presses the power button while in a
  // logged-in-as-a-non-guest-user, unlocked state.  When it fires, we lock the
  // screen.
  base::OneShotTimer<SessionStateController> lock_timer_;

  // Started when we request that the screen be locked.  When it fires, we
  // assume that our request got dropped.
  base::OneShotTimer<SessionStateController> lock_fail_timer_;

  // Started when the screen is locked while the power button is held.  Adds a
  // delay between the appearance of the lock screen and the beginning of the
  // pre-shutdown animation.
  base::OneShotTimer<SessionStateController> lock_to_shutdown_timer_;

  // Started when we begin displaying the pre-shutdown animation.  When it
  // fires, we start the shutdown animation and get ready to request shutdown.
  base::OneShotTimer<SessionStateController> pre_shutdown_timer_;

  // Started when we display the shutdown animation.  When it fires, we actually
  // request shutdown.  Gives the animation time to complete before Chrome, X,
  // etc. are shut down.
  base::OneShotTimer<SessionStateController> real_shutdown_timer_;

  scoped_ptr<internal::SessionStateAnimator> animator_;

  scoped_ptr<SessionStateControllerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateController);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_CONTROLLER_H_
