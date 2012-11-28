// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

#include <vector>

#include "ash/ash_export.h"

namespace aura {
class Window;
}
namespace base {
class TimeDelta;
}
namespace gfx {
class Rect;
}
namespace ui {
class ImplicitAnimationObserver;
class Layer;
class LayerAnimationSequence;
}

namespace ash {

// A variety of canned animations for window transitions.
enum WindowVisibilityAnimationType {
  WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT = 0,     // Default. Lets the system
                                                    // decide based on window
                                                    // type.
  WINDOW_VISIBILITY_ANIMATION_TYPE_DROP,            // Window shrinks in.
  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL,        // Vertical Glenimation.
  WINDOW_VISIBILITY_ANIMATION_TYPE_FADE,            // Fades in/out.
  WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE,        // Window scale/rotates down
                                                    // to its launcher icon.
  // Fade in/out using brightness and grayscale web filters.
  WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE,
  WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE,          // Window rotates in.
};

// Canned animations that take effect once but don't have a symmetric pair as
// visiblity animations do.
enum WindowAnimationType {
  WINDOW_ANIMATION_TYPE_BOUNCE = 0,  // Window scales up and down.
};

// Type of visibility change transition that a window should animate.
// Default behavior is to animate both show and hide.
enum WindowVisibilityAnimationTransition {
  ANIMATE_SHOW = 0x1,
  ANIMATE_HIDE = 0x2,
  ANIMATE_BOTH = ANIMATE_SHOW | ANIMATE_HIDE,
  ANIMATE_NONE = 0x4,
};

ASH_EXPORT void SetWindowVisibilityAnimationType(
    aura::Window* window,
    WindowVisibilityAnimationType type);

ASH_EXPORT WindowVisibilityAnimationType GetWindowVisibilityAnimationType(
    aura::Window* window);

ASH_EXPORT void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition);

ASH_EXPORT void SetWindowVisibilityAnimationDuration(
    aura::Window* window,
    const base::TimeDelta& duration);

ASH_EXPORT void SetWindowVisibilityAnimationVerticalPosition(
    aura::Window* window,
    float position);

// Creates an ImplicitAnimationObserver that takes ownership of the layers
// associated with a Window so that the animation can continue after the Window
// has been destroyed.
// The returned object deletes itself when the animations are done.
ASH_EXPORT ui::ImplicitAnimationObserver* CreateHidingWindowAnimationObserver(
    aura::Window* window);

// Animate a cross-fade of |window| from its current bounds to |new_bounds|.
ASH_EXPORT void CrossFadeToBounds(aura::Window* window,
                                  const gfx::Rect& new_bounds);

// Cross fades |layer| (which is a clone of |window|s layer before it was
// resized) to |window|s current bounds. |new_workspace| is the Window of the
// workspace |window| was added to.
// This takes ownership of |layer|.
ASH_EXPORT void CrossFadeWindowBetweenWorkspaces(aura::Window* new_workspace,
                                                 aura::Window* window,
                                                 ui::Layer* layer);

// Returns the duration of the cross-fade animation based on the |old_bounds|
// and |new_bounds| of the window.
ASH_EXPORT base::TimeDelta GetCrossFadeDuration(const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds);

namespace internal {

// Returns false if the |window| didn't animate.
ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                      bool visible);
ASH_EXPORT bool AnimateWindow(aura::Window* window, WindowAnimationType type);

// Creates vector of animation sequences that lasts for |duration| and changes
// brightness and grayscale to |target_value|. Caller takes ownership of
// returned LayerAnimationSequence objects.
ASH_EXPORT std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
