// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_POWER_BUTTON_CONTROLLER_H_
#define ASH_WM_POWER_BUTTON_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
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
class ASH_EXPORT PowerButtonController : public aura::RootWindowObserver,
                                         public ShellObserver {
 public:
  // Animations that can be applied to groups of containers.
  // Exposed here for TestApi::ContainersAreAnimated().
  enum AnimationType {
    ANIMATION_SLOW_CLOSE = 0,
    ANIMATION_UNDO_SLOW_CLOSE,
    ANIMATION_FAST_CLOSE,
    ANIMATION_FADE_IN,
    ANIMATION_HIDE,
    ANIMATION_RESTORE,
  };

  // Specific containers or groups of containers that can be animated.
  // Exposed here for TestApi::ContainersAreAnimated().
  enum Container {
    DESKTOP_BACKGROUND = 1 << 0,

    // All user session related containers including system background but
    // not including desktop background (wallpaper).
    NON_LOCK_SCREEN_CONTAINERS = 1 << 1,

    // Desktop wallpaper is moved to this layer when screen is locked.
    // This layer is excluded from lock animation so that wallpaper stays as is,
    // user session windows are hidden and lock UI is shown on top of it.
    // This layer is included in shutdown animation.
    LOCK_SCREEN_BACKGROUND = 1 << 2,

    // Lock screen and lock screen modal containers.
    LOCK_SCREEN_CONTAINERS = 1 << 3,

    // Multiple system layers belong here like status, menu, tooltip
    // and overlay layers.
    LOCK_SCREEN_RELATED_CONTAINERS = 1 << 4,
  };

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(PowerButtonController* controller)
        : controller_(controller) {}

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
    bool hide_black_layer_timer_is_running() const {
      return controller_->hide_black_layer_timer_.IsRunning();
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
    void trigger_hide_black_layer_timeout() {
      controller_->HideBlackLayer();
      controller_->hide_black_layer_timer_.Stop();
    }

    // Returns true if containers of a given |container_mask|
    // were last animated with |type| (probably; the analysis is fairly ad-hoc).
    // |container_mask| is a bitfield of a Container.
    bool ContainersAreAnimated(int container_mask, AnimationType type) const;

    // Returns true if |black_layer_| is non-NULL and visible.
    bool BlackLayerIsVisible() const;

    // Returns |black_layer_|'s bounds, or an empty rect if the layer is
    // NULL.
    gfx::Rect GetBlackLayerBounds() const;

   private:
    PowerButtonController* controller_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Helper method that returns a bitfield mask of all containers.
  static int GetAllContainersMask();

  // Helper method that returns a bitfield mask including LOCK_SCREEN_WALLPAPER,
  // LOCK_SCREEN_CONTAINERS, and LOCK_SCREEN_RELATED_CONTAINERS.
  static int GetAllLockScreenContainersMask();

  PowerButtonController();
  virtual ~PowerButtonController();

  void set_delegate(PowerButtonControllerDelegate* delegate) {
    delegate_.reset(delegate);
  }

  void set_has_legacy_power_button_for_test(bool legacy) {
    has_legacy_power_button_ = legacy;
  }

  // Called when the current screen brightness changes.
  void OnScreenBrightnessChanged(double percent);

  // Called when Chrome gets a request to display the lock screen.
  void OnStartingLock();

  // Called when the power or lock buttons are pressed or released.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Displays the shutdown animation and requests shutdown when it's done.
  void RequestShutdown();

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;
  virtual void OnRootWindowHostCloseRequested(
                                   const aura::RootWindow* root) OVERRIDE;

  // ShellObserver overrides:
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

 private:
  bool LoggedInAsNonGuest() const;

  // Requests that the screen be locked and starts |lock_fail_timer_|.
  void OnLockTimeout();

  // Aborts the pre-lock animation.
  void OnLockFailTimeout();

  // Displays the pre-shutdown animation and starts |shutdown_timer_|.
  void OnLockToShutdownTimeout();

  // Calls StartShutdownAnimationAndRequestShutdown().
  void OnShutdownTimeout();

  // Requests that the machine be shut down.
  void OnRealShutdownTimeout();

  // Puts us into the pre-lock or pre-shutdown state.
  void StartLockTimer();
  void StartShutdownTimer();

  // Displays the shutdown animation and starts |real_shutdown_timer_|.
  void StartShutdownAnimationAndRequestShutdown();

  // Shows or hides |black_layer_|.  The show method creates and
  // initializes the layer if it doesn't already exist.
  void ShowBlackLayer();
  void HideBlackLayer();

  scoped_ptr<PowerButtonControllerDelegate> delegate_;

  // The current login status.
  user::LoginStatus login_status_;

  // Original login status during locked.  LOGGED_IN_NONE if it's not locked.
  user::LoginStatus unlocked_login_status_;

  // Are the power or lock buttons currently held?
  bool power_button_down_;
  bool lock_button_down_;

  // Is the screen currently turned off?
  bool screen_is_off_;

  // Are we in the process of shutting the machine down?
  bool shutting_down_;

  // Was a command-line switch set telling us that we're running on hardware
  // that misreports power button releases?
  bool has_legacy_power_button_;

  // Layer that's stacked under all of the root window's children to provide a
  // black background when we're scaling all of the other windows down.
  // TODO(derat): Remove this in favor of having the compositor only clear the
  // viewport when there are regions not covered by a layer:
  // http://crbug.com/113445
  scoped_ptr<ui::Layer> black_layer_;

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
  // |black_layer_|, as the desktop background is now covering the whole
  // screen.
  base::OneShotTimer<PowerButtonController> hide_black_layer_timer_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonController);
};

}  // namespace ash

#endif  // ASH_WM_POWER_BUTTON_CONTROLLER_H_
