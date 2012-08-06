// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COORDINATE_CONVERSION_H_
#define ASH_WM_COORDINATE_CONVERSION_H_

#include "ash/ash_export.h"

#include "ui/gfx/point.h"

namespace aura {
class RootWindow;
class Window;
}  // namespace gfx

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
namespace wm {

// Returns the RootWindow at |point| in the virtual screen coordinates.
// Returns NULL if the root window does not exist at the given
// point.
ASH_EXPORT aura::RootWindow* GetRootWindowAt(const gfx::Point& point);

// Returns the RootWindow that shares the most area with |rect| in
// the virtual scren coordinates.
ASH_EXPORT aura::RootWindow* GetRootWindowMatching(const gfx::Rect& rect);

// Finds the root window at |location| in |window|'s coordinates and returns a
// pair of root window and location in that root window's coordinates. The
// function usually returns |window->GetRootWindow()|, but if the mouse pointer
// is moved outside the |window|'s root while the mouse is captured, it returns
// the other root window.
ASH_EXPORT std::pair<aura::RootWindow*, gfx::Point>
GetRootWindowRelativeToWindow(aura::Window* window, const gfx::Point& location);

// Converts the |point| from a given |window|'s coordinates into the screen
// coordinates.
ASH_EXPORT void ConvertPointToScreen(aura::Window* window, gfx::Point* point);

// Converts the |point| from the screen coordinates to a given |window|'s
// coordinates.
ASH_EXPORT void ConvertPointFromScreen(aura::Window* window,
                                       gfx::Point* point_in_screen);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COORDINATE_CONVERSION_H_
