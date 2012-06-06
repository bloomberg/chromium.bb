// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_
#pragma once

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
  WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW,  // Windows are scaled and
                                                    // fade in.
  WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE,  // Inverse of SHOW.
  WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE,        // Window scale/rotates down
                                                    // to its launcher icon.
};

// Type of visibility change transition that a window should animate.
// Default behavior is to animate both show and hide.
enum WindowVisibilityAnimationTransition {
  // 0 is used as default.
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

namespace internal {

// Animate a cross-fade of |window| from its current bounds to |new_bounds|.
ASH_EXPORT void CrossFadeToBounds(aura::Window* window,
                                  const gfx::Rect& new_bounds);

// Returns false if the |window| didn't animate.
ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(
    aura::Window* window, bool visible);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
