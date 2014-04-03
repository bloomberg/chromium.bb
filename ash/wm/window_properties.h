// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;

template<typename T>
struct WindowProperty;
}

namespace ash {
namespace wm {
class WindowState;
}  // namespace wm

// Shell-specific window property keys.

// Alphabetical sort.

// If this is set to true, the window stays in the same root window
// even if the bounds outside of its root window is set.
// This is exported as it's used in the tests.
ASH_EXPORT extern const aura::WindowProperty<bool>* const
    kStayInSameRootWindowKey;

// Property to tell if the container uses the screen coordinates.
extern const aura::WindowProperty<bool>* const kUsesScreenCoordinatesKey;

// A property key to store WindowState in the window. The window state
// is owned by the window.
extern const aura::WindowProperty<wm::WindowState*>* const kWindowStateKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
