// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_
#pragma once

#include <set>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// TODO(jamescook): Put all these functions in namespace window_util.

// Convenience setters/getters for |aura::client::kRootWindowActiveWindow|.
ASH_EXPORT void ActivateWindow(aura::Window* window);
ASH_EXPORT void DeactivateWindow(aura::Window* window);
ASH_EXPORT bool IsActiveWindow(aura::Window* window);
ASH_EXPORT aura::Window* GetActiveWindow();

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationController.
// If you're looking for a function to get the activatable "top level" window,
// this is probably what you're looking for.
ASH_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

namespace window_util {

// Returns true if |window| is in the maximized state.
ASH_EXPORT bool IsWindowMaximized(aura::Window* window);

// Returns true if |window| is in the fullscreen state.
ASH_EXPORT bool IsWindowFullscreen(aura::Window* window);

// Returns true if the set of |windows| contains a full-screen window.
typedef std::set<aura::Window*> WindowSet;
ASH_EXPORT bool HasFullscreenWindow(const WindowSet& windows);

}  // namespace window_util
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
