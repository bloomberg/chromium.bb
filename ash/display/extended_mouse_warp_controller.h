// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H
#define ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H

#include "ash/display/mouse_warp_controller.h"

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ash {
namespace test {
class DisplayManagerTestApi;
}
class SharedDisplayEdgeIndicator;

// A MouseWarpController used in extended display mode.
class ASH_EXPORT ExtendedMouseWarpController : public MouseWarpController {
 public:
  explicit ExtendedMouseWarpController(aura::Window* drag_source);
  ~ExtendedMouseWarpController() override;

  // MouseWarpController:
  bool WarpMouseCursor(ui::MouseEvent* event) override;
  void SetEnabled(bool enable) override;

 private:
  friend class test::DisplayManagerTestApi;
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           IndicatorBoundsTestOnRight);
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           IndicatorBoundsTestOnLeft);
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           IndicatorBoundsTestOnTopBottom);

  // Warps the mouse cursor to an alternate root window when the
  // mouse location in |event|, hits the edge of the event target's root and
  // the mouse cursor is considered to be in an alternate display.
  // If |update_mouse_location_now| is true,
  // Returns true if/ the cursor was moved.
  bool WarpMouseCursorInNativeCoords(const gfx::Point& point_in_native,
                                     const gfx::Point& point_in_screen,
                                     bool update_mouse_location_now);

  // Update the edge/indicator bounds based on the current
  // display configuration.
  void UpdateHorizontalEdgeBounds();
  void UpdateVerticalEdgeBounds();

  // Returns the source and destination window. When |src_window| is
  // |drag_soruce_root_| when it is set. Otherwise, the |src_window|
  // is always the primary root window, because there is no difference
  // between moving src to dst and moving dst to src.
  void GetSrcAndDstRootWindows(aura::Window** src_window,
                               aura::Window** dst_window);

  void allow_non_native_event_for_test() { allow_non_native_event_ = true; }

  // The bounds for warp hole windows. |dst_indicator_bounds_| is kept
  // in the instance for testing.
  gfx::Rect src_indicator_bounds_;
  gfx::Rect dst_indicator_bounds_;

  gfx::Rect src_edge_bounds_in_native_;
  gfx::Rect dst_edge_bounds_in_native_;

  // The root window in which the dragging started.
  aura::Window* drag_source_root_;

  bool enabled_;

  // Shows the area where a window can be dragged in to/out from
  // another display.
  scoped_ptr<SharedDisplayEdgeIndicator> shared_display_edge_indicator_;

  bool allow_non_native_event_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedMouseWarpController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H
