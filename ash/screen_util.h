// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_UTIL_H_
#define ASH_SCREEN_UTIL_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace gfx {
class Display;
class Rect;
class Point;
}

namespace display {
using Display = gfx::Display;
}

namespace ash {

class ASH_EXPORT ScreenUtil {
 public:
  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  static display::Display FindDisplayContainingPoint(const gfx::Point& point);

  // Returns the bounds for maximized windows in parent coordinates.
  // Maximized windows trigger auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window);

  // Returns the display bounds in parent coordinates.
  static gfx::Rect GetDisplayBoundsInParent(aura::Window* window);

  // Returns the display's work area bounds in parent coordinates.
  static gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window);

  // Returns the physical display bounds containing the shelf that
  // shares the same root window as |root|. Physical displays can
  // differ from logical displays in unified desktop mode.
  // TODO(oshima): If we need to expand the unified desktop support to
  // general use, we should consider always using physical display in
  // window layout instead of root window, and keep the logical
  // display only in display management code.
  static gfx::Rect GetShelfDisplayBoundsInRoot(aura::Window* window);

  // TODO(oshima): Move following two to wm/coordinate_conversion.h
  // Converts |rect| from |window|'s coordinates to the virtual screen
  // coordinates.
  static gfx::Rect ConvertRectToScreen(aura::Window* window,
                                       const gfx::Rect& rect);

  // Converts |rect| from virtual screen coordinates to the |window|'s
  // coordinates.
  static gfx::Rect ConvertRectFromScreen(aura::Window* window,
                                         const gfx::Rect& rect);

  // Returns a display::Display object for secondary display. Returns
  // invalid display if there is no secondary display connected.
  static const display::Display& GetSecondaryDisplay();

 private:
  ScreenUtil() {}
  ~ScreenUtil() {}

  DISALLOW_COPY_AND_ASSIGN(ScreenUtil);
};

}  // namespace ash

#endif  // ASH_SCREEN_UTIL_H_
