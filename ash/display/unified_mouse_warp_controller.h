// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_
#define ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_

#include "ash/display/mouse_warp_controller.h"

#include <stdint.h>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Point;
}

namespace ash {
namespace test {
class AshTestBase;
class DisplayManagerTestApi;
}

// A MouseWarpController used in unified display mode.
class ASH_EXPORT UnifiedMouseWarpController : public MouseWarpController {
 public:
  UnifiedMouseWarpController();
  ~UnifiedMouseWarpController() override;

  // MouseWarpController:
  bool WarpMouseCursor(ui::MouseEvent* event) override;
  void SetEnabled(bool enabled) override;

 private:
  friend class test::AshTestBase;
  friend class test::DisplayManagerTestApi;
  friend class UnifiedMouseWarpControllerTest;

  void ComputeBounds();

  // Warps the mouse cursor to an alternate root window when the
  // mouse location in |event|, hits the edge of the event target's root and
  // the mouse cursor is considered to be in an alternate display.
  // If |update_mouse_location_now| is true,
  // Returns true if/ the cursor was moved.
  bool WarpMouseCursorInNativeCoords(const gfx::Point& point_in_native,
                                     const gfx::Point& point_in_screen,
                                     bool update_mouse_location_now);

  void update_location_for_test() { update_location_for_test_ = true; }

  gfx::Rect first_edge_bounds_in_native_;
  gfx::Rect second_edge_bounds_in_native_;

  int64_t current_cursor_display_id_;

  bool update_location_for_test_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMouseWarpController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_UNIFIED_MOUSE_WARP_CONTROLLER_H_
