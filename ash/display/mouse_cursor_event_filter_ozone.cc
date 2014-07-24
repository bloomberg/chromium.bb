// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"

namespace ash {

void MouseCursorEventFilter::OnDisplayConfigurationChanged() {
  src_edge_bounds_in_native_.SetRect(0, 0, 0, 0);
  dst_edge_bounds_in_native_.SetRect(0, 0, 0, 0);
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessary(ui::MouseEvent* event) {
  gfx::Point point_in_screen(event->location());
  aura::Window* target = static_cast<aura::Window*>(event->target());
  wm::ConvertPointToScreen(target, &point_in_screen);
  return WarpMouseCursorInScreenCoords(target->GetRootWindow(),
                                       point_in_screen);
}

bool MouseCursorEventFilter::WarpMouseCursorInScreenCoords(
    aura::Window* target_root,
    const gfx::Point& point_in_screen) {
  if (Shell::GetScreen()->GetNumDisplays() <= 1 ||
      mouse_warp_mode_ == WARP_NONE)
    return false;

  // Do not warp again right after the cursor was warped. Sometimes the offset
  // is not long enough and the cursor moves at the edge of the destination
  // display. See crbug.com/278885
  // TODO(mukai): simplify the offset calculation below, it would not be
  // necessary anymore with this flag.
  if (was_mouse_warped_) {
    was_mouse_warped_ = false;
    return false;
  }

  aura::Window* root_at_point = wm::GetRootWindowAt(point_in_screen);
  gfx::Point point_in_root = point_in_screen;
  wm::ConvertPointFromScreen(root_at_point, &point_in_root);
  gfx::Rect root_bounds = root_at_point->bounds();
  int offset_x = 0;
  int offset_y = 0;

  // If the window is dragged between 2x display and 1x display,
  // staring from 2x display, pointer location is rounded by the
  // source scale factor (2x) so it will never reach the edge (which
  // is odd). Shrink by scale factor of the display where the dragging
  // started instead.  Only integral scale factor is supported for now.
  int shrink = scale_when_drag_started_;
  // Make the bounds inclusive to detect the edge.
  root_bounds.Inset(0, 0, shrink, shrink);
  gfx::Rect src_indicator_bounds = src_indicator_bounds_;
  src_indicator_bounds.Inset(-shrink, -shrink, -shrink, -shrink);

  if (point_in_root.x() <= root_bounds.x()) {
    // Use -2, not -1, to avoid infinite loop of pointer warp.
    offset_x = -2 * scale_when_drag_started_;
  } else if (point_in_root.x() >= root_bounds.right()) {
    offset_x = 2 * scale_when_drag_started_;
  } else if (point_in_root.y() <= root_bounds.y()) {
    offset_y = -2 * scale_when_drag_started_;
  } else if (point_in_root.y() >= root_bounds.bottom()) {
    offset_y = 2 * scale_when_drag_started_;
  } else {
    return false;
  }

  gfx::Point point_in_dst_screen(point_in_screen);
  point_in_dst_screen.Offset(offset_x, offset_y);
  aura::Window* dst_root = wm::GetRootWindowAt(point_in_dst_screen);

  // Warp the mouse cursor only if the location is in the indicator bounds
  // or the mouse pointer is in the destination root.
  if (mouse_warp_mode_ == WARP_DRAG &&
      dst_root != drag_source_root_ &&
      !src_indicator_bounds.Contains(point_in_screen)) {
    return false;
  }

  wm::ConvertPointFromScreen(dst_root, &point_in_dst_screen);

  if (dst_root->bounds().Contains(point_in_dst_screen)) {
    DCHECK_NE(dst_root, root_at_point);
    was_mouse_warped_ = true;
    dst_root->MoveCursorTo(point_in_dst_screen);
    return true;
  }
  return false;
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessaryForTest(
    aura::Window* target_root,
    const gfx::Point& point_in_screen) {
  return WarpMouseCursorInScreenCoords(target_root, point_in_screen);
}

}  // namespac
