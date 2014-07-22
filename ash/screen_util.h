// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_UTIL_H_
#define ASH_SCREEN_UTIL_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace aura {
class Window;
}

namespace gfx {
class Display;
class Rect;
class Point;
}

namespace ash {

class ASH_EXPORT ScreenUtil {
 public:
  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  static gfx::Display FindDisplayContainingPoint(const gfx::Point& point);

  // Returns the bounds for maximized windows in parent coordinates.
  // Maximized windows trigger auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window);

  // Returns the display bounds in parent coordinates.
  static gfx::Rect GetDisplayBoundsInParent(aura::Window* window);

  // Returns the display's work area bounds in parent coordinates.
  static gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window);

  // TODO(oshima): Move following two to wm/coordinate_conversion.h
  // Converts |rect| from |window|'s coordinates to the virtual screen
  // coordinates.
  static gfx::Rect ConvertRectToScreen(aura::Window* window,
                                       const gfx::Rect& rect);

  // Converts |rect| from virtual screen coordinates to the |window|'s
  // coordinates.
  static gfx::Rect ConvertRectFromScreen(aura::Window* window,
                                         const gfx::Rect& rect);

  // Returns a gfx::Display object for secondary display. Returns
  // invalid display if there is no secondary display connected.
  static const gfx::Display& GetSecondaryDisplay();

  // Returns a gfx::Display object for the specified id.  Returns
  // invalid display if no such display is connected.
  static const gfx::Display& GetDisplayForId(int64 display_id);

 private:
  ScreenUtil() {}
  ~ScreenUtil() {}

  DISALLOW_COPY_AND_ASSIGN(ScreenUtil);
};

}  // namespace ash

#endif  // ASH_SCREEN_UTIL_H_
