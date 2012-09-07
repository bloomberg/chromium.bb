// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_ASH_H_
#define ASH_SCREEN_ASH_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/gfx/screen_impl.h"

namespace gfx {
class Rect;
}

namespace ash {

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public gfx::ScreenImpl {
 public:
  ScreenAsh();
  virtual ~ScreenAsh();

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  static gfx::Display FindDisplayContainingPoint(const gfx::Point& point);

  // Returns the bounds for maximized windows in parent coordinates.
  // Maximized windows trigger auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window);

  // Returns work area when a maximized window is not present in
  // parent coordinates.
  static gfx::Rect GetUnmaximizedWorkAreaBoundsInParent(aura::Window* window);

  // Returns the display bounds in parent coordinates.
  static gfx::Rect GetDisplayBoundsInParent(aura::Window* window);

  // Returns the display's work area bounds in parent coordinates.
  static gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window);

  // Converts |rect| from |window|'s coordinates to the virtual screen
  // coordinates.
  static gfx::Rect ConvertRectToScreen(aura::Window* window,
                                       const gfx::Rect& rect);

  // Converts |rect| from virtual screen coordinates to the |window|'s
  // coordinates.
  static gfx::Rect ConvertRectFromScreen(aura::Window* window,
                                         const gfx::Rect& rect);

 protected:
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE;

  virtual int GetNumDisplays() OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_SCREEN_ASH_H_
