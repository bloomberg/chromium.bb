// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/extended_mouse_warp_controller.h"

#include <cmath>

#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/display/shared_display_edge_indicator.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

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

}  // namespace

ExtendedMouseWarpController::ExtendedMouseWarpController(
    aura::Window* drag_source)
    : drag_source_root_(drag_source),
      shared_display_edge_indicator_(new SharedDisplayEdgeIndicator),
      allow_non_native_event_(false) {
  DisplayLayout::Position position = Shell::GetInstance()
                                         ->display_manager()
                                         ->GetCurrentDisplayLayout()
                                         .position;
  // TODO(oshima): Use ComputeBondary instead.
  if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM)
    UpdateHorizontalEdgeBounds();
  else
    UpdateVerticalEdgeBounds();
  if (drag_source) {
    shared_display_edge_indicator_->Show(src_indicator_bounds_,
                                         dst_indicator_bounds_);
  }
}

ExtendedMouseWarpController::~ExtendedMouseWarpController() {
}

bool ExtendedMouseWarpController::WarpMouseCursor(ui::MouseEvent* event) {
  if (Shell::GetScreen()->GetNumDisplays() <= 1 || !enabled_)
    return false;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point point_in_screen = event->location();
  ::wm::ConvertPointToScreen(target, &point_in_screen);

  // A native event may not exist in unit test. Generate the native point
  // from the screen point instead.
  if (!event->HasNativeEvent()) {
    if (!allow_non_native_event_)
      return false;
    aura::Window* target_root = target->GetRootWindow();
    gfx::Point point_in_native = point_in_screen;
    ::wm::ConvertPointFromScreen(target_root, &point_in_native);
    target_root->GetHost()->ConvertPointToNativeScreen(&point_in_native);
    return WarpMouseCursorInNativeCoords(point_in_native, point_in_screen,
                                         true);
  }

  gfx::Point point_in_native =
      ui::EventSystemLocationFromNative(event->native_event());

#if defined(USE_OZONE)
  // TODO(dnicoara): crbug.com/415680 Move cursor warping into Ozone once Ozone
  // has access to the logical display layout.
  // Native events in Ozone are in the native window coordinate system. We need
  // to translate them to get the global position.
  point_in_native.Offset(target->GetHost()->GetBounds().x(),
                         target->GetHost()->GetBounds().y());
#endif

  return WarpMouseCursorInNativeCoords(point_in_native, point_in_screen, false);
}

void ExtendedMouseWarpController::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool ExtendedMouseWarpController::WarpMouseCursorInNativeCoords(
    const gfx::Point& point_in_native,
    const gfx::Point& point_in_screen,
    bool update_mouse_location_now) {
  bool in_src_edge = src_edge_bounds_in_native_.Contains(point_in_native);
  bool in_dst_edge = dst_edge_bounds_in_native_.Contains(point_in_native);
  if (!in_src_edge && !in_dst_edge)
    return false;

  // The mouse must move.
  aura::Window* src_root = nullptr;
  aura::Window* dst_root = nullptr;
  GetSrcAndDstRootWindows(&src_root, &dst_root);
  AshWindowTreeHost* target_ash_host =
      GetRootWindowController(in_src_edge ? dst_root : src_root)->ash_host();

  MoveCursorTo(target_ash_host, point_in_screen, update_mouse_location_now);
  return true;
}

void ExtendedMouseWarpController::UpdateHorizontalEdgeBounds() {
  bool from_primary = Shell::GetPrimaryRootWindow() == drag_source_root_;
  // GetPrimaryDisplay returns an object on stack, so copy the bounds
  // instead of using reference.
  const gfx::Rect primary_bounds =
      Shell::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Rect secondary_bounds = ScreenUtil::GetSecondaryDisplay().bounds();
  DisplayLayout::Position position = Shell::GetInstance()
                                         ->display_manager()
                                         ->GetCurrentDisplayLayout()
                                         .position;

  src_indicator_bounds_.set_x(
      std::max(primary_bounds.x(), secondary_bounds.x()));
  src_indicator_bounds_.set_width(
      std::min(primary_bounds.right(), secondary_bounds.right()) -
      src_indicator_bounds_.x());
  src_indicator_bounds_.set_height(kIndicatorThickness);
  src_indicator_bounds_.set_y(
      position == DisplayLayout::TOP
          ? primary_bounds.y() - (from_primary ? 0 : kIndicatorThickness)
          : primary_bounds.bottom() - (from_primary ? kIndicatorThickness : 0));

  dst_indicator_bounds_ = src_indicator_bounds_;
  dst_indicator_bounds_.set_height(kIndicatorThickness);
  dst_indicator_bounds_.set_y(
      position == DisplayLayout::TOP
          ? primary_bounds.y() - (from_primary ? kIndicatorThickness : 0)
          : primary_bounds.bottom() - (from_primary ? 0 : kIndicatorThickness));

  aura::Window* src_root = nullptr;
  aura::Window* dst_root = nullptr;
  GetSrcAndDstRootWindows(&src_root, &dst_root);

  src_edge_bounds_in_native_ = GetNativeEdgeBounds(
      GetRootWindowController(src_root)->ash_host(), src_indicator_bounds_);
  dst_edge_bounds_in_native_ = GetNativeEdgeBounds(
      GetRootWindowController(dst_root)->ash_host(), dst_indicator_bounds_);
}

void ExtendedMouseWarpController::UpdateVerticalEdgeBounds() {
  int snap_height = drag_source_root_ ? kMaximumSnapHeight : 0;
  bool in_primary = Shell::GetPrimaryRootWindow() == drag_source_root_;
  // GetPrimaryDisplay returns an object on stack, so copy the bounds
  // instead of using reference.
  const gfx::Rect primary_bounds =
      Shell::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Rect secondary_bounds = ScreenUtil::GetSecondaryDisplay().bounds();
  DisplayLayout::Position position = Shell::GetInstance()
                                         ->display_manager()
                                         ->GetCurrentDisplayLayout()
                                         .position;

  int upper_shared_y = std::max(primary_bounds.y(), secondary_bounds.y());
  int lower_shared_y =
      std::min(primary_bounds.bottom(), secondary_bounds.bottom());
  int shared_height = lower_shared_y - upper_shared_y;

  int dst_x =
      position == DisplayLayout::LEFT
          ? primary_bounds.x() - (in_primary ? kIndicatorThickness : 0)
          : primary_bounds.right() - (in_primary ? 0 : kIndicatorThickness);
  dst_indicator_bounds_.SetRect(dst_x, upper_shared_y, kIndicatorThickness,
                                shared_height);

  // The indicator on the source display.
  src_indicator_bounds_.set_width(kIndicatorThickness);
  src_indicator_bounds_.set_x(
      position == DisplayLayout::LEFT
          ? primary_bounds.x() - (in_primary ? 0 : kIndicatorThickness)
          : primary_bounds.right() - (in_primary ? kIndicatorThickness : 0));

  const gfx::Rect& source_bounds =
      in_primary ? primary_bounds : secondary_bounds;
  int upper_indicator_y = source_bounds.y() + snap_height;
  int lower_indicator_y = std::min(source_bounds.bottom(), lower_shared_y);

  // This gives a hight that can be used without sacrifying the snap space.
  int available_space =
      lower_indicator_y - std::max(upper_shared_y, upper_indicator_y);

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

  aura::Window* src_root = nullptr;
  aura::Window* dst_root = nullptr;
  GetSrcAndDstRootWindows(&src_root, &dst_root);

  // Native
  src_edge_bounds_in_native_ = GetNativeEdgeBounds(
      GetRootWindowController(src_root)->ash_host(), src_indicator_bounds_);
  dst_edge_bounds_in_native_ = GetNativeEdgeBounds(
      GetRootWindowController(dst_root)->ash_host(), dst_indicator_bounds_);
}

void ExtendedMouseWarpController::GetSrcAndDstRootWindows(
    aura::Window** src_root,
    aura::Window** dst_root) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  *src_root = drag_source_root_ ? drag_source_root_
                                : Shell::GetInstance()->GetPrimaryRootWindow();
  *dst_root = root_windows[0] == *src_root ? root_windows[1] : root_windows[0];
}

}  // namespace ash
