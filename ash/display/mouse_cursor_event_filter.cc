// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/display/display_controller.h"
#include "ash/display/shared_display_edge_indicator.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/cursor_manager.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/layout.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {
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
      drag_source_root_(NULL),
      shared_display_edge_indicator_(new SharedDisplayEdgeIndicator) {
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
  HideSharedEdgeIndicator();
}

void MouseCursorEventFilter::ShowSharedEdgeIndicator(
    const aura::RootWindow* from) {
  HideSharedEdgeIndicator();
  if (gfx::Screen::GetNumDisplays() <= 1 || from == NULL) {
    src_indicator_bounds_.SetRect(0, 0, 0, 0);
    dst_indicator_bounds_.SetRect(0, 0, 0, 0);
    drag_source_root_ = NULL;
    return;
  }
  drag_source_root_ = from;

  DisplayLayout::Position position = Shell::GetInstance()->
      display_controller()->default_display_layout().position;
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

bool MouseCursorEventFilter::PreHandleKeyEvent(aura::Window* target,
                                               ui::KeyEvent* event) {
  return false;
}

bool MouseCursorEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                 ui::MouseEvent* event) {
  // Handle both MOVED and DRAGGED events here because when the mouse pointer
  // enters the other root window while dragging, the underlying window system
  // (at least X11) stops generating a ui::ET_MOUSE_MOVED event.
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED) {
      return false;
  }

  aura::RootWindow* current_root = target->GetRootWindow();
  gfx::Point location_in_root(event->location());
  aura::Window::ConvertPointToTarget(target, current_root, &location_in_root);
  return WarpMouseCursorIfNecessary(current_root, location_in_root);
}

ui::TouchStatus MouseCursorEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult MouseCursorEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessary(
    aura::RootWindow* current_root,
    const gfx::Point& point_in_root) {
  if (gfx::Screen::GetNumDisplays() <= 1 || mouse_warp_mode_ == WARP_NONE)
    return false;
  const float scale = ui::GetDeviceScaleFactor(current_root->layer());

  // The pointer might be outside the |current_root|. Get the root window where
  // the pointer is currently on.
  std::pair<aura::RootWindow*, gfx::Point> actual_location =
      wm::GetRootWindowRelativeToWindow(current_root, point_in_root);
  current_root = actual_location.first;
  // Don't use |point_in_root| below. Instead, use |actual_location.second|
  // which is in |actual_location.first|'s coordinates.

  gfx::Rect root_bounds = current_root->bounds();
  int offset_x = 0;
  int offset_y = 0;
  if (actual_location.second.x() <= root_bounds.x()) {
    // Use -2, not -1, to avoid infinite loop of pointer warp.
    offset_x = -2 * scale;
  } else if (actual_location.second.x() >= root_bounds.right() - 1) {
    offset_x = 2 * scale;
  } else if (actual_location.second.y() <= root_bounds.y()) {
    offset_y = -2 * scale;
  } else if (actual_location.second.y() >= root_bounds.bottom() - 1) {
    offset_y = 2 * scale;
  } else {
    return false;
  }

  gfx::Point point_in_screen(actual_location.second);
  wm::ConvertPointToScreen(current_root, &point_in_screen);
  gfx::Point point_in_dst_screen(point_in_screen);
  point_in_dst_screen.Offset(offset_x, offset_y);
  aura::RootWindow* dst_root = wm::GetRootWindowAt(point_in_dst_screen);

  // Warp the mouse cursor only if the location is in the indicator bounds
  // or the mouse pointer is in the destination root.
  if (mouse_warp_mode_ == WARP_DRAG &&
      dst_root != drag_source_root_ &&
      !src_indicator_bounds_.Contains(point_in_screen)) {
    return false;
  }

  wm::ConvertPointFromScreen(dst_root, &point_in_dst_screen);

  if (dst_root->bounds().Contains(point_in_dst_screen)) {
    DCHECK_NE(dst_root, current_root);
    dst_root->MoveCursorTo(point_in_dst_screen);
    ash::Shell::GetInstance()->cursor_manager()->SetDeviceScaleFactor(
        dst_root->AsRootWindowHostDelegate()->GetDeviceScaleFactor());
    return true;
  }
  return false;
}

void MouseCursorEventFilter::UpdateHorizontalIndicatorWindowBounds() {
  bool from_primary = Shell::GetPrimaryRootWindow() == drag_source_root_;

  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Rect& primary_bounds = display_manager->GetDisplayAt(0)->bounds();
  const gfx::Rect& secondary_bounds =
      display_manager->GetDisplayAt(1)->bounds();
  DisplayLayout::Position position = Shell::GetInstance()->
      display_controller()->default_display_layout().position;

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
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Rect& primary_bounds = display_manager->GetDisplayAt(0)->bounds();
  const gfx::Rect& secondary_bounds =
      display_manager->GetDisplayAt(1)->bounds();
  DisplayLayout::Position position = Shell::GetInstance()->
      display_controller()->default_display_layout().position;

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

}  // namespace internal
}  // namespace ash
