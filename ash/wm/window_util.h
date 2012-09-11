// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class Layer;
}

namespace ash {
namespace wm {

// Convenience setters/getters for |aura::client::kRootWindowActiveWindow|.
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

// Returns true if |window| is normal or default.
ASH_EXPORT bool IsWindowNormal(aura::Window* window);

// Returns true if |state| is normal or default.
ASH_EXPORT bool IsWindowStateNormal(ui::WindowShowState state);

// Returns true if |window| is in the maximized state.
ASH_EXPORT bool IsWindowMaximized(aura::Window* window);

// Returns true if |window| is minimized.
ASH_EXPORT bool IsWindowMinimized(aura::Window* window);

// Returns true if |window| is in the fullscreen state.
ASH_EXPORT bool IsWindowFullscreen(aura::Window* window);

// Maximizes |window|, which must not be NULL.
ASH_EXPORT void MaximizeWindow(aura::Window* window);

// Minimizes |window|, which must not be NULL.
ASH_EXPORT void MinimizeWindow(aura::Window* window);

// Restores |window|, which must not be NULL.
ASH_EXPORT void RestoreWindow(aura::Window* window);

// Moves the window to the center of the display.
ASH_EXPORT void CenterWindow(aura::Window* window);

// Returns the existing Layer for |window| (and all its descendants) and creates
// a new layer for |window| and all its descendants. This is intended for
// animations that want to animate between the existing visuals and a new window
// state. The caller owns the return value.
//
// As a result of this |window| has freshly created layers, meaning the layers
// are all empty (nothing has been painted to them) and are sized to 0x0. Soon
// after this call you need to reset the bounds of the window. Or, you can pass
// true as the second argument to let the function do that.
ASH_EXPORT ui::Layer* RecreateWindowLayers(aura::Window* window,
                                           bool set_bounds) WARN_UNUSED_RESULT;

// Deletes |layer| and all its child layers.
ASH_EXPORT void DeepDeleteLayers(ui::Layer* layer);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
