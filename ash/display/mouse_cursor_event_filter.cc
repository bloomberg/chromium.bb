// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/shared_display_edge_indicator.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/layout.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace {

// Maximum size on the display edge that initiate snapping phantom window,
// from the corner of the display.
const int kMaximumSnapHeight = 16;

// Minimum height of an indicator on the display edge that allows
// dragging a window.  If two displays shares the edge smaller than
// this, entire edge will be used as a draggable space.
const int kMinimumIndicatorHeight = 200;

const int kIndicatorThickness = 1;
}

MouseCursorEventFilter::MouseCursorEventFilter()
    : mouse_warp_mode_(WARP_ALWAYS),
      was_mouse_warped_(false),
      drag_source_root_(NULL),
      scale_when_drag_started_(1.0f),
      shared_display_edge_indicator_(new SharedDisplayEdgeIndicator) {
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
  HideSharedEdgeIndicator();
}

void MouseCursorEventFilter::ShowSharedEdgeIndicator(
    const aura::Window* from) {
  HideSharedEdgeIndicator();
  if (Shell::GetScreen()->GetNumDisplays() <= 1 || from == NULL) {
    src_indicator_bounds_.SetRect(0, 0, 0, 0);
    dst_indicator_bounds_.SetRect(0, 0, 0, 0);
    drag_source_root_ = NULL;
    return;
  }
  drag_source_root_ = from;

  DisplayLayout::Position position = Shell::GetInstance()->
      display_manager()->GetCurrentDisplayLayout().position;
  if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM)
    UpdateHorizontalIndicatorWindowBounds();
  else
    UpdateVerticalIndicatorWindowBounds();

  shared_display_edge_indicator_->Show(src_indicator_bounds_,
                                       dst_indicator_bounds_);
}

void MouseCursorEventFilter::HideSharedEdgeIndicator() {
  shared_display_edge_indicator_->Hide();
}

void MouseCursorEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  RootWindowController* rwc = GetRootWindowController(target->GetRootWindow());
  // The root window controller is removed during the shutting down
  // RootWindow, so don't process events futher.
  if (!rwc) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSE_PRESSED) {
    scale_when_drag_started_ = ui::GetDeviceScaleFactor(target->layer());
  } else if (event->type() == ui::ET_MOUSE_RELEASED) {
    scale_when_drag_started_ = 1.0f;
  }

  // Handle both MOVED and DRAGGED events here because when the mouse pointer
  // enters the other root window while dragging, the underlying window system
  // (at least X11) stops generating a ui::ET_MOUSE_MOVED event.
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED) {
      return;
  }

  if (!(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    Shell::GetInstance()->display_controller()->
        cursor_window_controller()->UpdateLocation();
  }

  gfx::Point point_in_screen(event->location());
  wm::ConvertPointToScreen(target, &point_in_screen);
  if (WarpMouseCursorIfNecessary(target->GetRootWindow(), point_in_screen))
    event->StopPropagation();
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessary(
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

void MouseCursorEventFilter::UpdateHorizontalIndicatorWindowBounds() {
  bool from_primary = Shell::GetPrimaryRootWindow() == drag_source_root_;
  // GetPrimaryDisplay returns an object on stack, so copy the bounds
  // instead of using reference.
  const gfx::Rect primary_bounds =
      Shell::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Rect secondary_bounds = ScreenUtil::GetSecondaryDisplay().bounds();
  DisplayLayout::Position position = Shell::GetInstance()->
      display_manager()->GetCurrentDisplayLayout().position;

  src_indicator_bounds_.set_x(
      std::max(primary_bounds.x(), secondary_bounds.x()));
  src_indicator_bounds_.set_width(
      std::min(primary_bounds.right(), secondary_bounds.right()) -
      src_indicator_bounds_.x());
  src_indicator_bounds_.set_height(kIndicatorThickness);
  src_indicator_bounds_.set_y(
      position == DisplayLayout::TOP ?
      primary_bounds.y() - (from_primary ? 0 : kIndicatorThickness) :
      primary_bounds.bottom() - (from_primary ? kIndicatorThickness : 0));

  dst_indicator_bounds_ = src_indicator_bounds_;
  dst_indicator_bounds_.set_height(kIndicatorThickness);
  dst_indicator_bounds_.set_y(
      position == DisplayLayout::TOP ?
      primary_bounds.y() - (from_primary ? kIndicatorThickness : 0) :
      primary_bounds.bottom() - (from_primary ? 0 : kIndicatorThickness));
}

void MouseCursorEventFilter::UpdateVerticalIndicatorWindowBounds() {
  bool in_primary = Shell::GetPrimaryRootWindow() == drag_source_root_;
  // GetPrimaryDisplay returns an object on stack, so copy the bounds
  // instead of using reference.
  const gfx::Rect primary_bounds =
      Shell::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Rect secondary_bounds = ScreenUtil::GetSecondaryDisplay().bounds();
  DisplayLayout::Position position = Shell::GetInstance()->
      display_manager()->GetCurrentDisplayLayout().position;

  int upper_shared_y = std::max(primary_bounds.y(), secondary_bounds.y());
  int lower_shared_y = std::min(primary_bounds.bottom(),
                                secondary_bounds.bottom());
  int shared_height = lower_shared_y - upper_shared_y;

  int dst_x = position == DisplayLayout::LEFT ?
      primary_bounds.x() - (in_primary ? kIndicatorThickness : 0) :
      primary_bounds.right() - (in_primary ? 0 : kIndicatorThickness);
  dst_indicator_bounds_.SetRect(
      dst_x, upper_shared_y, kIndicatorThickness, shared_height);

  // The indicator on the source display.
  src_indicator_bounds_.set_width(kIndicatorThickness);
  src_indicator_bounds_.set_x(
      position == DisplayLayout::LEFT ?
      primary_bounds.x() - (in_primary ? 0 : kIndicatorThickness) :
      primary_bounds.right() - (in_primary ? kIndicatorThickness : 0));

  const gfx::Rect& source_bounds =
      in_primary ? primary_bounds : secondary_bounds;
  int upper_indicator_y = source_bounds.y() + kMaximumSnapHeight;
  int lower_indicator_y = std::min(source_bounds.bottom(), lower_shared_y);

  // This gives a hight that can be used without sacrifying the snap space.
  int available_space = lower_indicator_y -
      std::max(upper_shared_y, upper_indicator_y);

  if (shared_height < kMinimumIndicatorHeight) {
    // If the shared height is smaller than minimum height, use the
    // entire height.
    upper_indicator_y = upper_shared_y;
  } else if (available_space < kMinimumIndicatorHeight) {
    // Snap to the bottom.
    upper_indicator_y =
        std::max(upper_shared_y, lower_indicator_y + kMinimumIndicatorHeight);
  } else {
    upper_indicator_y = std::max(upper_indicator_y, upper_shared_y);
  }
  src_indicator_bounds_.set_y(upper_indicator_y);
  src_indicator_bounds_.set_height(lower_indicator_y - upper_indicator_y);
}

}  // namespace ash
