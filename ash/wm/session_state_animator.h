// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Layer;
}

namespace ash {
namespace internal {

// Displays onscreen animations for session state changes (lock/unlock, sign
// out, shut down).
class ASH_EXPORT SessionStateAnimator : public aura::RootWindowObserver {
 public:
  // Animations that can be applied to groups of containers.
  enum AnimationType {
    ANIMATION_PARTIAL_CLOSE = 0,
    ANIMATION_UNDO_PARTIAL_CLOSE,
    ANIMATION_FULL_CLOSE,
    ANIMATION_FADE_IN,
    ANIMATION_HIDE,
    ANIMATION_RESTORE,
  };

  // Specific containers or groups of containers that can be animated.
  enum Container {
    DESKTOP_BACKGROUND = 1 << 0,
    LAUNCHER = 1 << 1,

    // All user session related containers including system background but
    // not including desktop background (wallpaper).
    NON_LOCK_SCREEN_CONTAINERS = 1 << 2,

    // Desktop wallpaper is moved to this layer when screen is locked.
    // This layer is excluded from lock animation so that wallpaper stays as is,
    // user session windows are hidden and lock UI is shown on top of it.
    // This layer is included in shutdown animation.
    LOCK_SCREEN_BACKGROUND = 1 << 3,

    // Lock screen and lock screen modal containers.
    LOCK_SCREEN_CONTAINERS = 1 << 4,

    // Multiple system layers belong here like status, menu, tooltip
    // and overlay layers.
    LOCK_SCREEN_RELATED_CONTAINERS = 1 << 5,
  };

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateAnimator* animator)
        : animator_(animator) {}

    bool hide_black_layer_timer_is_running() const {
      return animator_->hide_black_layer_timer_.IsRunning();
    }

    void TriggerHideBlackLayerTimeout();

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
    SessionStateAnimator* animator_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // A bitfield mask including LOCK_SCREEN_WALLPAPER,
  // LOCK_SCREEN_CONTAINERS, and LOCK_SCREEN_RELATED_CONTAINERS.
  const static int kAllLockScreenContainersMask;

  // A bitfield mask of all containers.
  const static int kAllContainersMask;

  SessionStateAnimator();
  virtual ~SessionStateAnimator();

  // Shows or hides |black_layer_|.  The show method creates and
  // initializes the layer if it doesn't already exist.
  void ShowBlackLayer();
  void DropBlackLayer();

  // Drops back layer after |UNDO_SLOW_CLOSE| animation delay.
  void ScheduleDropBlackLayer();

  // Apply animation |type| to all containers included in |container_mask|.
  void StartAnimation(int container_mask,
                      AnimationType type);

  // Fills |containers| with the containers included in |container_mask|.
  void GetContainers(int container_mask,
                     aura::Window::Windows* containers);

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

 private:
  // Layer that's stacked under all of the root window's children to provide a
  // black background when we're scaling all of the other windows down.
  // TODO(derat): Remove this in favor of having the compositor only clear the
  // viewport when there are regions not covered by a layer:
  // http://crbug.com/113445
  scoped_ptr<ui::Layer> black_layer_;

  // Started when we abort the pre-lock state.  When it fires, we hide
  // |black_layer_|, as the desktop background is now covering the whole
  // screen.
  base::OneShotTimer<SessionStateAnimator> hide_black_layer_timer_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateAnimator);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_ANIMATOR_H_
