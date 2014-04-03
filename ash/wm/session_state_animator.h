// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Layer;
}

namespace ash {

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
    ANIMATION_FADE_OUT,
    ANIMATION_HIDE_IMMEDIATELY,
    ANIMATION_RESTORE,
    // Animations that raise/lower windows to/from area "in front" of the
    // screen.
    ANIMATION_LIFT,
    ANIMATION_UNDO_LIFT,
    ANIMATION_DROP,
    // Animations that raise/lower windows from/to area "behind" of the screen.
    ANIMATION_RAISE_TO_SCREEN,
    ANIMATION_LOWER_BELOW_SCREEN,
    ANIMATION_PARTIAL_FADE_IN,
    ANIMATION_UNDO_PARTIAL_FADE_IN,
    ANIMATION_FULL_FADE_IN,
    ANIMATION_GRAYSCALE_BRIGHTNESS,
    ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
  };

  // Constants for determining animation speed.
  enum AnimationSpeed {
    // Immediately change state.
    ANIMATION_SPEED_IMMEDIATE = 0,
    // Speed for animations associated with user action that can be undone.
    // Used for pre-lock and pre-shutdown animations.
    ANIMATION_SPEED_UNDOABLE,
    // Speed for animation that reverts undoable action. Used for aborting
    // pre-lock and pre-shutdown animations.
    ANIMATION_SPEED_REVERT,
    // Speed for user action that can not be undone, Used for lock and shutdown
    // animations requested via menus/shortcuts and for animating remaining
    // parts of partial lock/shutdown animations.
    ANIMATION_SPEED_FAST,
    // Speed for lock screen appearance in "old" animation set.
    ANIMATION_SPEED_SHOW_LOCK_SCREEN,
    // Speed for workspace-like animations in "new" animation set.
    ANIMATION_SPEED_MOVE_WINDOWS,
    // Speed for undoing workspace-like animations in "new" animation set.
    ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
    // Speed for shutdown in "new" animation set.
    ANIMATION_SPEED_SHUTDOWN,
    // Speed for reverting shutdown in "new" animation set.
    ANIMATION_SPEED_REVERT_SHUTDOWN,
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

    // Returns true if containers of a given |container_mask|
    // were last animated with |type| (probably; the analysis is fairly ad-hoc).
    // |container_mask| is a bitfield of a Container.
    bool ContainersAreAnimated(int container_mask, AnimationType type) const;

    // Returns true if root window was last animated with |type| (probably;
    // the analysis is fairly ad-hoc).
    bool RootWindowIsAnimated(AnimationType type) const;

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

  // Reports animation duration for |speed|.
  static base::TimeDelta GetDuration(AnimationSpeed speed);

  // Fills |containers| with the containers included in |container_mask|.
  static void GetContainers(int container_mask,
                            aura::Window::Windows* containers);

  // Apply animation |type| to all containers included in |container_mask| with
  // specified |speed|.
  void StartAnimation(int container_mask,
                      AnimationType type,
                      AnimationSpeed speed);

  // Apply animation |type| to all containers included in |container_mask| with
  // specified |speed| and call a |callback| at the end of the animation, if it
  // is not null.
  void StartAnimationWithCallback(int container_mask,
                                  AnimationType type,
                                  AnimationSpeed speed,
                                  base::Callback<void(void)>& callback);

//  Apply animation |type| to all containers included in |container_mask| with
// specified |speed| and add |observer| to all animations.
  void StartAnimationWithObserver(int container_mask,
                                  AnimationType type,
                                  AnimationSpeed speed,
                                  ui::LayerAnimationObserver* observer);

  // Applies animation |type| whith specified |speed| to the root container.
  void StartGlobalAnimation(AnimationType type,
                            AnimationSpeed speed);

  // Apply animation |type| to window |window| with |speed| and add |observer|
  // if it is not NULL to the last animation sequence.
  void RunAnimationForWindow(aura::Window* window,
                             AnimationType type,
                             AnimationSpeed speed,
                             ui::LayerAnimationObserver* observer);

  DISALLOW_COPY_AND_ASSIGN(SessionStateAnimator);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_ANIMATOR_H_
