// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_CONTROLLER_IMPL_H_
#define ASH_WM_SESSION_STATE_CONTROLLER_IMPL_H_

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
class PowerButtonControllerTest;
}

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
class ASH_EXPORT SessionStateControllerImpl : public SessionStateController {
 public:

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateControllerImpl* controller);

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
    SessionStateControllerImpl* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  SessionStateControllerImpl();
  virtual ~SessionStateControllerImpl();

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
  friend class test::PowerButtonControllerTest;

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

  // The current login status, or original login status from before we locked..
  user::LoginStatus login_status_;

  // Current lock status.
  bool system_is_locked_;

  // Are we in the process of shutting the machine down?
  bool shutting_down_;

  // Indicates whether controller should proceed to (cancellable) shutdown after
  // locking.
  bool shutdown_after_lock_;

  // Started when the user first presses the power button while in a
  // logged-in-as-a-non-guest-user, unlocked state.  When it fires, we lock the
  // screen.
  base::OneShotTimer<SessionStateControllerImpl> lock_timer_;

  // Started when we request that the screen be locked.  When it fires, we
  // assume that our request got dropped.
  base::OneShotTimer<SessionStateControllerImpl> lock_fail_timer_;

  // Started when the screen is locked while the power button is held.  Adds a
  // delay between the appearance of the lock screen and the beginning of the
  // pre-shutdown animation.
  base::OneShotTimer<SessionStateControllerImpl> lock_to_shutdown_timer_;

  // Started when we begin displaying the pre-shutdown animation.  When it
  // fires, we start the shutdown animation and get ready to request shutdown.
  base::OneShotTimer<SessionStateControllerImpl> pre_shutdown_timer_;

  // Started when we display the shutdown animation.  When it fires, we actually
  // request shutdown.  Gives the animation time to complete before Chrome, X,
  // etc. are shut down.
  base::OneShotTimer<SessionStateControllerImpl> real_shutdown_timer_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerImpl);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_CONTROLLER_IMPL_H_
