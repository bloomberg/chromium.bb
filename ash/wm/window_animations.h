// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

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
  // Fade in/out using brightness and grayscale web filters.
  WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE,
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

// Animate a cross-fade of |window| from its current bounds to |new_bounds|.
ASH_EXPORT void CrossFadeToBounds(aura::Window* window,
                                  const gfx::Rect& new_bounds);

// Cross fade |layer| (which is a clone of |window|s layer before it was
// resized) to windows current bounds. |new_workspace| is the original workspace
// |window| was in and |new_workspace| the new workspace.
// This takes ownership of |layer|.
ASH_EXPORT void CrossFadeWindowBetweenWorkspaces(aura::Window* old_workspace,
                                                 aura::Window* new_workspace,
                                                 aura::Window* window,
                                                 ui::Layer* layer);

// Indicates the direction the workspace should appear to go.
enum WorkspaceAnimationDirection {
  WORKSPACE_ANIMATE_UP,
  WORKSPACE_ANIMATE_DOWN,
};

enum WorkspaceType {
  WORKSPACE_MAXIMIZED,
  WORKSPACE_DESKTOP,
};

// Amount of time for the workspace switch animation.
extern const int kWorkspaceSwitchTimeMS;

// Animates between two workspaces. If |animate_old| is false |old_window| is
// not animated, otherwise it is. |is_restoring_maximized_window| is true if
// the switch is the result of a minmized window being restored.
ASH_EXPORT void AnimateBetweenWorkspaces(aura::Window* old_window,
                                         WorkspaceType old_type,
                                         bool animate_old,
                                         aura::Window* new_window,
                                         WorkspaceType new_type,
                                         bool is_restoring_maximized_window);

// Animates the workspace visualy in or out. This is used when the workspace is
// becoming active, and out when the workspace was active.
ASH_EXPORT void AnimateWorkspaceIn(aura::Window* window,
                                   WorkspaceAnimationDirection direction);
ASH_EXPORT void AnimateWorkspaceOut(aura::Window* window,
                                    WorkspaceAnimationDirection direction,
                                    WorkspaceType type);

// Returns the amount of time before destroying the system background.
ASH_EXPORT base::TimeDelta GetSystemBackgroundDestroyDuration();

namespace internal {

// Returns the duration of the cross-fade animation based on the |old_bounds|
// and |new_bounds| of the window.
ASH_EXPORT base::TimeDelta GetCrossFadeDuration(const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds);

// Returns false if the |window| didn't animate.
ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(
    aura::Window* window, bool visible);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
