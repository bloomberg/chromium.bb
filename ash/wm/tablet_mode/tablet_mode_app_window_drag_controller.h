// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/wm_toplevel_window_event_handler.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {
class TabletModeWindowDragDelegate;

// Handles app windows dragging in tablet mode. App windows can be dragged into
// splitscreen through swiping from the top of the screen in tablet mode.
class ASH_EXPORT TabletModeAppWindowDragController {
 public:
  // Threshold of the fling velocity to drop the window into overview.
  static constexpr float kFlingToOverviewThreshold = 100.0f;

  TabletModeAppWindowDragController();
  ~TabletModeAppWindowDragController();

  // Processes a gesture event and updates the transform of |dragged_window_|.
  // Returns true if the gesture has been handled and it should not be processed
  // any further, false otherwise. Depending on the event position, the dragged
  // window may be 1) maximized, or 2) snapped in splitscren.
  bool DragWindowFromTop(ui::GestureEvent* event);

 private:
  // Gesture window drag related functions. Used in DragWindowFromTop.
  void StartWindowDrag(ui::GestureEvent* event);
  void UpdateWindowDrag(ui::GestureEvent* event);
  void EndWindowDrag(ui::GestureEvent* event,
                     wm::WmToplevelWindowEventHandler::DragResult result);

  // Apply transform to the dragged window during dragging.
  void UpdateDraggedWindow(const gfx::Point& location_in_screen);

  // The scale of the window during drag is based on the distance from
  // |y_location_in_screen| to the y position of |new_selector_item_bounds_|.
  // The dragged window will become smaller when it becomes nearer to the new
  // selector item. And then keep the minimum scale if it has been dragged
  // further than the new selector item.
  float CalculateWindowScale(int y_location_in_screen) const;

  std::unique_ptr<TabletModeWindowDragDelegate> drag_delegate_;

  gfx::Point initial_location_in_screen_;

  // Overview mode will be triggered if a window is being dragged from the top
  // of the display, and a new selector item will be created in the overview
  // grid. See |new_selector_item_widget_| in WindowGrid for more info.
  gfx::Rect new_selector_item_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeAppWindowDragController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_APP_WINDOW_DRAG_CONTROLLER_H_
