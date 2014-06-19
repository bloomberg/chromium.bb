// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include <cmath>

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/shared_display_edge_indicator.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
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

// This is to disable the new mouse warp logic in case
// it caused the problem in the branch.
// Events from Ozone don't have a native event
#if defined(USE_OZONE)
bool enable_mouse_warp_in_native_coords = false;
#else
bool enable_mouse_warp_in_native_coords = true;
#endif

void ConvertPointFromScreenToNative(const aura::Window* root_window,
                                    gfx::Point* point) {
  wm::ConvertPointFromScreen(root_window, point);
  root_window->GetHost()->ConvertPointToNativeScreen(point);
}

gfx::Rect GetNativeEdgeBounds(const aura::Window* root_window,
                              gfx::Point start,
                              gfx::Point end) {
  gfx::Rect native_bounds = root_window->GetHost()->GetBounds();
  native_bounds.Inset(
      GetRootWindowController(root_window)->ash_host()->GetHostInsets());

  ConvertPointFromScreenToNative(root_window, &start);
  ConvertPointFromScreenToNative(root_window, &end);
  if (start.x() == end.x()) {
    // vertical in native
    int x = std::abs(native_bounds.x() - start.x()) <
                    std::abs(native_bounds.right() - start.x())
                ? native_bounds.x()
                : native_bounds.right() - 1;
    return gfx::Rect(
        x, std::min(start.y(), end.y()), 1, std::abs(start.y() - end.y()));
  } else {
    // horizontal in native
    int y = std::abs(native_bounds.y() - start.y()) <
                    std::abs(native_bounds.bottom() - start.y())
                ? native_bounds.y()
                : native_bounds.bottom() - 1;
    return gfx::Rect(
        std::min(start.x(), end.x()), y, std::abs(start.x() - end.x()), 1);
  }
}

// Creates edge bounds from indicator bounds that fits the edge
// of the native window for |root_window|.
gfx::Rect CreateVerticalEdgeBoundsInNative(const aura::Window* root_window,
                                           const gfx::Rect& indicator_bounds) {
  gfx::Point start = indicator_bounds.origin();
  gfx::Point end = start;
  end.set_y(indicator_bounds.bottom());
  return GetNativeEdgeBounds(root_window, start, end);
}

gfx::Rect CreateHorizontalEdgeBoundsInNative(
    const aura::Window* root_window,
    const gfx::Rect& indicator_bounds) {
  gfx::Point start = indicator_bounds.origin();
  gfx::Point end = start;
  end.set_x(indicator_bounds.right());
  return GetNativeEdgeBounds(root_window, start, end);
}

void MovePointInside(const gfx::Rect& native_bounds,
                     gfx::Point* point_in_native) {
  if (native_bounds.x() > point_in_native->x())
    point_in_native->set_x(native_bounds.x());
  if (native_bounds.right() < point_in_native->x())
    point_in_native->set_x(native_bounds.right());

  if (native_bounds.y() > point_in_native->y())
    point_in_native->set_y(native_bounds.y());
  if (native_bounds.bottom() < point_in_native->y())
    point_in_native->set_y(native_bounds.bottom());
}

// Moves the cursor to the point inside the root that is closest to
// the point_in_screen, which is outside of the root window.
void MoveCursorTo(aura::Window* root, const gfx::Point& point_in_screen) {
  gfx::Point point_in_native = point_in_screen;
  wm::ConvertPointFromScreen(root, &point_in_native);
  root->GetHost()->ConvertPointToNativeScreen(&point_in_native);

  // now fit the point inside the native bounds.
  gfx::Rect native_bounds = root->GetHost()->GetBounds();
  gfx::Point native_origin = native_bounds.origin();
  native_bounds.Inset(
      GetRootWindowController(root)->ash_host()->GetHostInsets());
  // Shrink further so that the mouse doesn't warp on the
  // edge. The right/bottom needs to be shrink by 2 to subtract
  // the 1 px from width/height value.
  native_bounds.Inset(1, 1, 2, 2);

  MovePointInside(native_bounds, &point_in_native);
  gfx::Point point_in_host = point_in_native;

  point_in_host.Offset(-native_origin.x(), -native_origin.y());
  root->GetHost()->MoveCursorToHostLocation(point_in_host);
}

}  // namespace

// static
bool MouseCursorEventFilter::IsMouseWarpInNativeCoordsEnabled() {
  return enable_mouse_warp_in_native_coords;
}

MouseCursorEventFilter::MouseCursorEventFilter()
    : mouse_warp_mode_(WARP_ALWAYS),
      was_mouse_warped_(false),
      drag_source_root_(NULL),
      scale_when_drag_started_(1.0f),
      shared_display_edge_indicator_(new SharedDisplayEdgeIndicator) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

MouseCursorEventFilter::~MouseCursorEventFilter() {
  HideSharedEdgeIndicator();
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void MouseCursorEventFilter::ShowSharedEdgeIndicator(aura::Window* from) {
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
    UpdateHorizontalEdgeBounds();
  else
    UpdateVerticalEdgeBounds();

  shared_display_edge_indicator_->Show(src_indicator_bounds_,
                                       dst_indicator_bounds_);
}

void MouseCursorEventFilter::HideSharedEdgeIndicator() {
  shared_display_edge_indicator_->Hide();
  OnDisplayConfigurationChanged();
}

void MouseCursorEventFilter::OnDisplaysInitialized() {
  OnDisplayConfigurationChanged();
}

void MouseCursorEventFilter::OnDisplayConfigurationChanged() {
  // Extra check for |num_connected_displays()| is for SystemDisplayApiTest
  // that injects MockScreen.
  if (Shell::GetScreen()->GetNumDisplays() <= 1 ||
      Shell::GetInstance()->display_manager()->num_connected_displays() <= 1 ||
      !enable_mouse_warp_in_native_coords) {
    src_edge_bounds_in_native_.SetRect(0, 0, 0, 0);
    dst_edge_bounds_in_native_.SetRect(0, 0, 0, 0);
    return;
  }

  drag_source_root_ = NULL;
  DisplayLayout::Position position = Shell::GetInstance()
                                         ->display_manager()
                                         ->GetCurrentDisplayLayout()
                                         .position;

  if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM)
    UpdateHorizontalEdgeBounds();
  else
    UpdateVerticalEdgeBounds();
}

void MouseCursorEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());

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

  Shell::GetInstance()->display_controller()->
      cursor_window_controller()->UpdateLocation();

  if (WarpMouseCursorIfNecessary(event))
    event->StopPropagation();
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessary(ui::MouseEvent* event) {
  if (enable_mouse_warp_in_native_coords) {
    if (!event->HasNativeEvent())
      return false;

    gfx::Point point_in_native =
        ui::EventSystemLocationFromNative(event->native_event());

    gfx::Point point_in_screen = event->location();
    aura::Window* target = static_cast<aura::Window*>(event->target());
    wm::ConvertPointToScreen(target, &point_in_screen);

    return WarpMouseCursorInNativeCoords(point_in_native, point_in_screen);
  } else {
    gfx::Point point_in_screen(event->location());
    aura::Window* target = static_cast<aura::Window*>(event->target());
    wm::ConvertPointToScreen(target, &point_in_screen);
    return WarpMouseCursorInScreenCoords(target->GetRootWindow(),
                                         point_in_screen);
  }
}

bool MouseCursorEventFilter::WarpMouseCursorInNativeCoords(
    const gfx::Point& point_in_native,
    const gfx::Point& point_in_screen) {
  if (Shell::GetScreen()->GetNumDisplays() <= 1 ||
      mouse_warp_mode_ == WARP_NONE)
    return false;

  bool in_src_edge = src_edge_bounds_in_native_.Contains(point_in_native);
  bool in_dst_edge = dst_edge_bounds_in_native_.Contains(point_in_native);
  if (!in_src_edge && !in_dst_edge)
    return false;

  // The mouse must move.
  aura::Window* src_root = NULL;
  aura::Window* dst_root = NULL;
  GetSrcAndDstRootWindows(&src_root, &dst_root);

  if (in_src_edge)
    MoveCursorTo(dst_root, point_in_screen);
  else
    MoveCursorTo(src_root, point_in_screen);

  return true;
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

void MouseCursorEventFilter::UpdateHorizontalEdgeBounds() {
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

  aura::Window* src_root = NULL;
  aura::Window* dst_root = NULL;
  GetSrcAndDstRootWindows(&src_root, &dst_root);

  src_edge_bounds_in_native_ =
      CreateHorizontalEdgeBoundsInNative(src_root, src_indicator_bounds_);
  dst_edge_bounds_in_native_ =
      CreateHorizontalEdgeBoundsInNative(dst_root, dst_indicator_bounds_);
}

void MouseCursorEventFilter::UpdateVerticalEdgeBounds() {
  int snap_height = drag_source_root_ ? kMaximumSnapHeight : 0;
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
  int upper_indicator_y = source_bounds.y() + snap_height;
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

  aura::Window* src_root = NULL;
  aura::Window* dst_root = NULL;
  GetSrcAndDstRootWindows(&src_root, &dst_root);

  // Native
  src_edge_bounds_in_native_ =
      CreateVerticalEdgeBoundsInNative(src_root, src_indicator_bounds_);
  dst_edge_bounds_in_native_ =
      CreateVerticalEdgeBoundsInNative(dst_root, dst_indicator_bounds_);
}

void MouseCursorEventFilter::GetSrcAndDstRootWindows(aura::Window** src_root,
                                                     aura::Window** dst_root) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  *src_root = drag_source_root_ ? drag_source_root_
                                : Shell::GetInstance()->GetPrimaryRootWindow();
  *dst_root = root_windows[0] == *src_root ? root_windows[1] : root_windows[0];
}

bool MouseCursorEventFilter::WarpMouseCursorIfNecessaryForTest(
    aura::Window* target_root,
    const gfx::Point& point_in_screen) {
  if (!enable_mouse_warp_in_native_coords)
    return WarpMouseCursorInScreenCoords(target_root, point_in_screen);
  gfx::Point native = point_in_screen;
  wm::ConvertPointFromScreen(target_root, &native);
  target_root->GetHost()->ConvertPointToNativeScreen(&native);
  return WarpMouseCursorInNativeCoords(native, point_in_screen);
}

}  // namespace ash
