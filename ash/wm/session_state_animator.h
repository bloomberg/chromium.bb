// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/colored_window_controller.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
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
class ASH_EXPORT SessionStateAnimator {
 public:
  // Animations that can be applied to groups of containers.
  enum AnimationType {
    ANIMATION_PARTIAL_CLOSE = 0,
    ANIMATION_UNDO_PARTIAL_CLOSE,
    ANIMATION_FULL_CLOSE,
    ANIMATION_FADE_IN,
    ANIMATION_HIDE,
    ANIMATION_RESTORE,
    ANIMATION_RAISE,
    ANIMATION_LOWER,
    ANIMATION_PARTIAL_FADE_IN,
    ANIMATION_UNDO_PARTIAL_FADE_IN,
    ANIMATION_FULL_FADE_IN,
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

    LOCK_SCREEN_SYSTEM_FOREGROUND = 1 << 6,
  };

  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateAnimator* animator)
        : animator_(animator) {}

    // Returns true if containers of a given |container_mask|
    // were last animated with |type| (probably; the analysis is fairly ad-hoc).
    // |container_mask| is a bitfield of a Container.
    bool ContainersAreAnimated(int container_mask, AnimationType type) const;

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

  // Create |foreground_| layer if it doesn't already exist, but makes it
  // completely transparent.
  void CreateForeground();
  // Destroy |foreground_| when it is not needed anymore.
  void DropForeground();

  // Apply animation |type| to all containers included in |container_mask|.
  void StartAnimation(int container_mask,
                      AnimationType type);

  // Apply animation |type| to all containers included in |container_mask| and
  // call a |callback| at the end of the animation, if it is not null.
  void StartAnimationWithCallback(int container_mask,
                                  AnimationType type,
                                  base::Callback<void(void)>& callback);

  // Fills |containers| with the containers included in |container_mask|.
  void GetContainers(int container_mask,
                     aura::Window::Windows* containers);

  // Apply animation |type| to window |window| and add |observer| if it is not
  // NULL to the last animation sequence.
  void RunAnimationForWindow(aura::Window* window,
                             AnimationType type,
                             ui::LayerAnimationObserver* observer);

  // White foreground that is used during shutdown animation to "fade
  // everything into white".
  scoped_ptr<ColoredWindowController> foreground_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateAnimator);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_ANIMATOR_H_
