// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
#define ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {
class SharedDisplayEdgeIndicator;

// An event filter that controls mouse location in extended desktop
// environment.
class ASH_EXPORT MouseCursorEventFilter : public ui::EventHandler,
                                          public DisplayController::Observer {
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
  void ShowSharedEdgeIndicator(aura::Window* from);
  void HideSharedEdgeIndicator();

  // DisplayController::Observer:
  virtual void OnDisplaysInitialized() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

 private:
  friend class DragWindowResizerTest;
  friend class MouseCursorEventFilterTest;
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, DoNotWarpTwice);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest, SetMouseWarpModeFlag);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnRight);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnLeft);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           IndicatorBoundsTestOnTopBottom);
  FRIEND_TEST_ALL_PREFIXES(MouseCursorEventFilterTest,
                           WarpMouseDifferentScaleDisplaysInNative);

  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, WarpMousePointer);

  // Moves the cursor to the point inside the root that is closest to
  // the point_in_screen, which is outside of the root window.
  static void MoveCursorTo(aura::Window* root,
                           const gfx::Point& point_in_screen);

  // Warps the mouse cursor to an alternate root window when the
  // mouse location in |event|, hits the edge of the event target's root and
  // the mouse cursor is considered to be in an alternate display.
  // Returns true if/ the cursor was moved.
  bool WarpMouseCursorIfNecessary(ui::MouseEvent* event);

#if defined(USE_OZONE)
  bool WarpMouseCursorInScreenCoords(aura::Window* target_root,
                                     const gfx::Point& point_in_screen);

#else
  bool WarpMouseCursorInNativeCoords(const gfx::Point& point_in_native,
                                     const gfx::Point& point_in_screen);
#endif

  // Update the edge/indicator bounds based on the current
  // display configuration.
  void UpdateHorizontalEdgeBounds();
  void UpdateVerticalEdgeBounds();

  // Returns the source and destination window. When the
  // mouse_warp_mode_ is WARP_DRAG, src_window is the root window
  // where the drag starts. When the mouse_warp_mode_ is WARP_ALWAYS,
  // the src_window is always the primary root window, because there
  // is no difference between moving src to dst and moving dst to src.
  void GetSrcAndDstRootWindows(aura::Window** src_window,
                               aura::Window** dst_window);

  void reset_was_mouse_warped_for_test() { was_mouse_warped_ = false; }

  bool WarpMouseCursorIfNecessaryForTest(aura::Window* target_root,
                                         const gfx::Point& point_in_screen);

  MouseWarpMode mouse_warp_mode_;

  // This flag is used to suppress the accidental mouse warp back to the
  // original display.
  bool was_mouse_warped_;

  // The bounds for warp hole windows. |dst_indicator_bounds_| is kept
  // in the instance for testing.
  gfx::Rect src_indicator_bounds_;
  gfx::Rect dst_indicator_bounds_;

  gfx::Rect src_edge_bounds_in_native_;
  gfx::Rect dst_edge_bounds_in_native_;

  // The root window in which the dragging started.
  aura::Window* drag_source_root_;

  float scale_when_drag_started_;

  // Shows the area where a window can be dragged in to/out from
  // another display.
  scoped_ptr<SharedDisplayEdgeIndicator> shared_display_edge_indicator_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorEventFilter);
};

}  // namespace ash

#endif  // ASH_DISPLAY_MOUSE_CURSOR_EVENT_FILTER_H
