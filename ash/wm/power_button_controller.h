// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_POWER_BUTTON_CONTROLLER_H_
#define ASH_WM_POWER_BUTTON_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/aura/root_window_observer.h"

namespace gfx {
class Size;
}

namespace ui {
class Layer;
class LayerDelegate;
}

namespace ash {

// Performs system-related functions on behalf of PowerButtonController.
class ASH_EXPORT PowerButtonControllerDelegate {
 public:
  PowerButtonControllerDelegate() {}
  virtual ~PowerButtonControllerDelegate() {}

  virtual void RequestLockScreen() = 0;
  virtual void RequestShutdown() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerDelegate);
};

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
class ASH_EXPORT PowerButtonController : public aura::RootWindowObserver {
 public:
  // Animations that can be applied to groups of containers.
  // Exposed here for TestApi::ContainerGroupIsAnimated().
  enum AnimationType {
    ANIMATION_SLOW_CLOSE = 0,
    ANIMATION_UNDO_SLOW_CLOSE,
    ANIMATION_FAST_CLOSE,
    ANIMATION_FADE_IN,
    ANIMATION_HIDE,
    ANIMATION_RESTORE,
  };

  // Groups of containers that can be animated.
  // Exposed here for TestApi::ContainerGroupIsAnimated().
  enum ContainerGroup {
    ALL_CONTAINERS = 0,
    SCREEN_LOCKER_CONTAINERS,
    SCREEN_LOCKER_AND_RELATED_CONTAINERS,
    ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
  };

  // Helper class used by tests to access internal state.
  class TestApi {
   public:
    TestApi(PowerButtonController* controller) : controller_(controller) {}

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
      return controller_->shutdown_timer_.IsRunning();
    }
    bool real_shutdown_timer_is_running() const {
      return controller_->real_shutdown_timer_.IsRunning();
    }
    bool hide_background_layer_timer_is_running() const {
      return controller_->hide_background_layer_timer_.IsRunning();
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
      controller_->OnShutdownTimeout();
      controller_->shutdown_timer_.Stop();
    }
    void trigger_real_shutdown_timeout() {
      controller_->OnRealShutdownTimeout();
      controller_->real_shutdown_timer_.Stop();
    }
    void trigger_hide_background_layer_timeout() {
      controller_->HideBackgroundLayer();
      controller_->hide_background_layer_timer_.Stop();
    }

    // Returns true if the given set of containers was last animated with
    // |type| (probably; the analysis is fairly ad-hoc).
    bool ContainerGroupIsAnimated(ContainerGroup group,
                                  AnimationType type) const;

    // Returns true if |background_layer_| is non-NULL and visible.
    bool BackgroundLayerIsVisible() const;

   private:
    PowerButtonController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  PowerButtonController();
  virtual ~PowerButtonController();

  void set_delegate(PowerButtonControllerDelegate* delegate) {
    delegate_.reset(delegate);
  }

  // Called when the user logs in.
  void OnLoginStateChange(bool logged_in, bool is_guest);

  // Called when the screen is locked (after the lock window is visible) or
  // unlocked.
  void OnLockStateChange(bool locked);

  // Called when Chrome gets a request to display the lock screen.
  void OnStartingLock();

  // Called when the power or lock buttons are pressed or released.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const gfx::Size& new_size) OVERRIDE;

 private:
  class BackgroundLayerDelegate;

  // Requests that the screen be locked and starts |lock_fail_timer_|.
  void OnLockTimeout();

  // Aborts the pre-lock animation.
  void OnLockFailTimeout();

  // Displays the pre-shutdown animation and starts |shutdown_timer_|.
  void OnLockToShutdownTimeout();

  // Displays the shutdown animation and starts |real_shutdown_timer_|.
  void OnShutdownTimeout();

  // Requests that the machine be shut down.
  void OnRealShutdownTimeout();

  // Puts us into the pre-lock or pre-shutdown state.
  void StartLockTimer();
  void StartShutdownTimer();

  // Shows or hides |background_layer_|.  The show method creates and
  // initializes the layer if it doesn't already exist.
  void ShowBackgroundLayer();
  void HideBackgroundLayer();

  scoped_ptr<PowerButtonControllerDelegate> delegate_;

  // True if a non-guest user is currently logged in.
  bool logged_in_as_non_guest_;

  // True if the screen is currently locked.
  bool locked_;

  // Are the power or lock buttons currently held?
  bool power_button_down_;
  bool lock_button_down_;

  // Are we in the process of shutting the machine down?
  bool shutting_down_;

  // Should we start |shutdown_timer_| when we receive notification that the
  // screen has been locked?
  bool should_start_shutdown_timer_after_lock_;

  // Responsible for painting |background_layer_|.
  scoped_ptr<BackgroundLayerDelegate> background_layer_delegate_;

  // Layer that's stacked under all of the root window's children to provide a
  // black background when we're scaling all of the other windows down.
  scoped_ptr<ui::Layer> background_layer_;

  // Started when the user first presses the power button while in a
  // logged-in-as-a-non-guest-user, unlocked state.  When it fires, we lock the
  // screen.
  base::OneShotTimer<PowerButtonController> lock_timer_;

  // Started when we request that the screen be locked.  When it fires, we
  // assume that our request got dropped.
  base::OneShotTimer<PowerButtonController> lock_fail_timer_;

  // Started when the screen is locked while the power button is held.  Adds a
  // delay between the appearance of the lock screen and the beginning of the
  // pre-shutdown animation.
  base::OneShotTimer<PowerButtonController> lock_to_shutdown_timer_;

  // Started when we begin displaying the pre-shutdown animation.  When it
  // fires, we start the shutdown animation and get ready to request shutdown.
  base::OneShotTimer<PowerButtonController> shutdown_timer_;

  // Started when we display the shutdown animation.  When it fires, we actually
  // request shutdown.  Gives the animation time to complete before Chrome, X,
  // etc. are shut down.
  base::OneShotTimer<PowerButtonController> real_shutdown_timer_;

  // Started when we abort the pre-lock state.  When it fires, we hide
  // |background_layer_|, as the desktop background is now covering the whole
  // screen.
  base::OneShotTimer<PowerButtonController> hide_background_layer_timer_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace ash

#endif  // ASH_WM_POWER_BUTTON_CONTROLLER_H_
