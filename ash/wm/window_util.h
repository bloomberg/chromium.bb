// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace ui {
class Event;
}

namespace ash {
namespace wm {

class WmWindow;

// Utility functions for window activation.
ASH_EXPORT void ActivateWindow(aura::Window* window);
ASH_EXPORT void DeactivateWindow(aura::Window* window);
ASH_EXPORT bool IsActiveWindow(aura::Window* window);
ASH_EXPORT aura::Window* GetActiveWindow();
ASH_EXPORT bool CanActivateWindow(aura::Window* window);

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationController.
// If you're looking for a function to get the activatable "top level" window,
// this is probably what you're looking for.
ASH_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

// TODO(oshima): remove this.
ASH_EXPORT bool IsWindowMinimized(aura::Window* window);

// Returns true if |window|'s location can be controlled by the user.
ASH_EXPORT bool IsWindowUserPositionable(aura::Window* window);

// Moves the window to the center of the display.
ASH_EXPORT void CenterWindow(aura::Window* window);

ASH_EXPORT gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(
    wm::WmWindow* window);
ASH_EXPORT gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(
    wm::WmWindow* window);

// Returns the bounds of a left snapped window with default width in parent
// coordinates.
ASH_EXPORT gfx::Rect GetDefaultLeftSnappedWindowBoundsInParent(
    aura::Window* window);

// Returns the bounds of a right snapped window with default width in parent
// coordinates.
ASH_EXPORT gfx::Rect GetDefaultRightSnappedWindowBoundsInParent(
    aura::Window* window);

// Moves |window| to the root window where the |event| occured if it is not
// already in the same root window. Returns true if |window| was moved.
ASH_EXPORT bool MoveWindowToEventRoot(aura::Window* window,
                                      const ui::Event& event);

// Changes the parent of a |child| and all its transient children that are
// themselves children of |old_parent| to |new_parent|.
void ReparentChildWithTransientChildren(aura::Window* child,
                                        aura::Window* old_parent,
                                        aura::Window* new_parent);

// Changes the parent of all transient children of a |child| to |new_parent|.
// Does not change parent of the transient children that are not themselves
// children of |old_parent|.
void ReparentTransientChildrenOfChild(aura::Window* child,
                                      aura::Window* old_parent,
                                      aura::Window* new_parent);

// Snap the window's layer to physical pixel boundary.
void SnapWindowToPixelBoundary(aura::Window* window);

// Mark the container window so that InstallSnapLayoutManagerToContainers
// installs the SnapToPixelLayoutManager.
ASH_EXPORT void SetSnapsChildrenToPhysicalPixelBoundary(
    aura::Window* container);

// Traverse the |container| tree and installs SnapToPixelLayoutManager.
void InstallSnapLayoutManagerToContainers(aura::Window* container);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
