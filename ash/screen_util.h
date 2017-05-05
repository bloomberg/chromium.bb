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
class Rect;
}

namespace ash {

class ASH_EXPORT ScreenUtil {
 public:
  // Returns the bounds for maximized windows in parent coordinates.
  // Maximized windows trigger auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window);

  // Returns the display bounds in parent coordinates.
  static gfx::Rect GetDisplayBoundsInParent(aura::Window* window);

  // Returns the display's work area bounds in parent coordinates.
  static gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window);

  // Returns the bounds of the physical display containing the shelf for
  // |window|. Physical displays can differ from logical displays in unified
  // desktop mode.
  // TODO(oshima): Consider using physical displays in window layout, instead of
  // root windows, and only use logical display in display management code.
  static gfx::Rect GetDisplayBoundsWithShelf(aura::Window* window);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScreenUtil);
};

}  // namespace ash

#endif  // ASH_SCREEN_UTIL_H_
