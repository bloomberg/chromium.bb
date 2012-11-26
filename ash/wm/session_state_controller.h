// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_CONTROLLER_H_
#define ASH_WM_SESSION_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_observer.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
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
class SessionStateControllerImpl2Test;
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

  // Additional time (beyond kFastCloseAnimMs) to wait after starting the
  // fast-close shutdown animation before actually requesting shutdown, to give
  // the animation time to finish.
  static const int kShutdownRequestDelayMs;

  SessionStateController();
  virtual ~SessionStateController();

  void SetDelegate(SessionStateControllerDelegate* delegate);

  // Starts locking (with slow animation) that can be cancelled.
  // After locking and |kLockToShutdownTimeoutMs| StartShutdownAnimation()
  // will be called unless CancelShutdownAnimation() is called, if
  // |shutdown_after_lock| is true.
  virtual void StartLockAnimation(bool shutdown_after_lock) = 0;

  // Starts shutting down (with slow animation) that can be cancelled.
  virtual void StartShutdownAnimation() = 0;

  // Starts usual lock animation, but locks immediately.
  // Unlike StartLockAnimation it does no lead to StartShutdownAnimation.
  virtual void StartLockAnimationAndLockImmediately() = 0;

  // Returns true if we have requested system to lock, but haven't received
  // confirmation yet.
  virtual bool LockRequested() = 0;

  // Returns true if we are shutting down.
  virtual bool ShutdownRequested() = 0;

  // Returns true if we are within cancellable lock timeframe.
  virtual bool CanCancelLockAnimation() = 0;

  // Cancels locking and reverts lock animation.
  virtual void CancelLockAnimation() = 0;

  // Returns true if we are within cancellable shutdown timeframe.
  virtual bool CanCancelShutdownAnimation() = 0;

  // Cancels shutting down and reverts shutdown animation.
  virtual void CancelShutdownAnimation() = 0;

  // Called when Chrome gets a request to display the lock screen.
  virtual void OnStartingLock() = 0;

  // Displays the shutdown animation and requests shutdown when it's done.
  virtual void RequestShutdown() = 0;

  // Called when ScreenLocker is ready to close, but not yet destroyed.
  // Can be used to display "hiding" animations on unlock.
  // |callback| will be called when all animations are done.
  virtual void OnLockScreenHide(base::Closure& callback) = 0;

  // Sets up the callback that should be called once lock animation is finished.
  // Callback is guaranteed to be called once and then discarded.
  virtual void SetLockScreenDisplayedCallback(base::Closure& callback) = 0;

  virtual void AddObserver(SessionStateObserver* observer);
  virtual void RemoveObserver(SessionStateObserver* observer);
  virtual bool HasObserver(SessionStateObserver* observer);

 protected:
  friend class test::PowerButtonControllerTest;
  friend class test::SessionStateControllerImpl2Test;

  scoped_ptr<internal::SessionStateAnimator> animator_;

  scoped_ptr<SessionStateControllerDelegate> delegate_;

  ObserverList<SessionStateObserver> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateController);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_CONTROLLER_H_
