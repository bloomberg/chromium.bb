// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H
#define ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H

#include "ash/display/mouse_warp_controller.h"

#include <vector>

#include "ash/display/display_layout.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Display;
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
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           IndicatorBoundsTestThreeDisplays);

  // Defined in header file because tests need access.
  class WarpRegion {
   public:
    WarpRegion(int64_t a_display_id,
               int64_t b_display_id,
               const gfx::Rect& a_indicator_bounds,
               const gfx::Rect& b_indicator_bounds);
    ~WarpRegion();

    const gfx::Rect& a_indicator_bounds() { return a_indicator_bounds_; }
    const gfx::Rect& b_indicator_bounds() { return b_indicator_bounds_; }

   private:
    friend class ExtendedMouseWarpController;

    // If the mouse cursor is in |a_edge_bounds_in_native|, then it will be
    // moved to |b_display_id|. Similarily, if the cursor is in
    // |b_edge_bounds_in_native|, then it will be moved to |a_display_id|.

    // The id for the displays. Used for warping the cursor.
    int64_t a_display_id_;
    int64_t b_display_id_;

    gfx::Rect a_edge_bounds_in_native_;
    gfx::Rect b_edge_bounds_in_native_;

    // The bounds for warp hole windows. These are kept in the instance for
    // testing.
    gfx::Rect a_indicator_bounds_;
    gfx::Rect b_indicator_bounds_;

    // Shows the area where a window can be dragged in to/out from another
    // display.
    scoped_ptr<SharedDisplayEdgeIndicator> shared_display_edge_indicator_;

    DISALLOW_COPY_AND_ASSIGN(WarpRegion);
  };

  std::vector<scoped_ptr<WarpRegion>> warp_regions_;

  // Registers the WarpRegion; also displays a drag indicator on the screen if
  // |has_drag_source| is true.
  void AddWarpRegion(scoped_ptr<WarpRegion> region, bool has_drag_source);

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
  scoped_ptr<WarpRegion> CreateHorizontalEdgeBounds(
      const gfx::Display& a,
      const gfx::Display& b,
      DisplayPlacement::Position position);
  scoped_ptr<WarpRegion> CreateVerticalEdgeBounds(
      const gfx::Display& a,
      const gfx::Display& b,
      DisplayPlacement::Position position);

  void allow_non_native_event_for_test() { allow_non_native_event_ = true; }

  // The root window in which the dragging started.
  aura::Window* drag_source_root_;

  bool enabled_;

  bool allow_non_native_event_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedMouseWarpController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_EXTENDED_MOUSE_WARP_CONTROLLER_H
