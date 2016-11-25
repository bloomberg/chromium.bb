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

namespace ui {
class Event;
}

namespace ash {

namespace wm {

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

// Returns true if |window|'s location can be controlled by the user.
ASH_EXPORT bool IsWindowUserPositionable(aura::Window* window);

// Pins the window on top of other windows.
ASH_EXPORT void PinWindow(aura::Window* window, bool trusted);

// Moves |window| to the root window where the |event| occured if it is not
// already in the same root window. Returns true if |window| was moved.
ASH_EXPORT bool MoveWindowToEventRoot(aura::Window* window,
                                      const ui::Event& event);

// Snap the window's layer to physical pixel boundary.
void SnapWindowToPixelBoundary(aura::Window* window);

// Mark the container window so that InstallSnapLayoutManagerToContainers
// installs the SnapToPixelLayoutManager.
ASH_EXPORT void SetSnapsChildrenToPhysicalPixelBoundary(
    aura::Window* container);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
