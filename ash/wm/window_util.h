// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_
#pragma once

#include "ash/ash_export.h"

namespace aura {
class Window;
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

// Moves the window to the center of the monitor.
ASH_EXPORT void CenterWindow(aura::Window* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
