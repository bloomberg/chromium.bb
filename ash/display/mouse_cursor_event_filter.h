// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
#define ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
}

namespace ash {
class DisplayController;

namespace internal {
class SharedDisplayEdgeIndicator;

// An event filter that controls mouse location in extended desktop
// environment.
class ASH_EXPORT MouseCursorEventFilter : public aura::EventFilter {
 public:
  enum MouseWarpMode {
    WARP_ALWAYS,   // Always warp the mouse when possible.
    WARP_DRAG,     // Used when dragging a window. Top and bottom
                   // corner of the shared edge is reserved for window
                   // snapping.
    WARP_NONE,     // No mouse warping. Used when resizing the window.
  };

  MouseCursorEventFilter();
  virtual ~MouseCursorEventFilter();

  void set_mouse_warp_mode(MouseWarpMode mouse_warp_mode) {
    mouse_warp_mode_ = mouse_warp_mode;
  }

  // Shows/Hide the indicator for window dragging. The |from|
  // is the window where the dragging started.
  void ShowSharedEdgeIndicator(const aura::RootWindow* from);
  void HideSharedEdgeIndicator();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, SetMouseWarpModeFlag);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, WarpMouse);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           WarpMouseDifferentSizeDisplays);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnRight);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnLeft);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnTopBottom);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, WarpMousePointer);

  // Warps the mouse cursor to an alternate root window when the
  // |point_in_root|, which is the location of the mouse cursor,
  // hits or exceeds the edge of the |root_window| and the mouse cursor
  // is considered to be in an alternate display. Returns true if
  // the cursor was moved.
  bool WarpMouseCursorIfNecessary(aura::RootWindow* root_window,
                                  const gfx::Point& point_in_root);

  void UpdateHorizontalIndicatorWindowBounds();
  void UpdateVerticalIndicatorWindowBounds();

  MouseWarpMode mouse_warp_mode_;

  // The bounds for warp hole windows. |dst_indicator_bounds_| is kept
  // in the instance for testing.
  gfx::Rect src_indicator_bounds_;
  gfx::Rect dst_indicator_bounds_;

  // The root window in which the dragging started.
  const aura::RootWindow* drag_source_root_;

  // Shows the area where a window can be dragged in to/out from
  // another display.
  scoped_ptr<SharedDisplayEdgeIndicator> shared_display_edge_indicator_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
