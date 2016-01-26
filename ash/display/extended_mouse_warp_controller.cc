// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/extended_mouse_warp_controller.h"

#include <cmath>

#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/display/shared_display_edge_indicator.h"
#include "ash/display/window_tree_host_manager.h"
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

// Helper method that maps a gfx::Display to an aura::Window.
aura::Window* GetRootWindowForDisplayId(int64_t display_id) {
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->GetRootWindowForDisplayId(display_id);
}

// Helper method that maps an aura::Window to a gfx::Display.
gfx::Display GetDisplayFromWindow(aura::Window* window) {
  return Shell::GetScreen()->GetDisplayNearestWindow(window);
}

}  // namespace

ExtendedMouseWarpController::WarpRegion::WarpRegion(
    int64_t a_display_id,
    int64_t b_display_id,
    const gfx::Rect& a_indicator_bounds,
    const gfx::Rect& b_indicator_bounds)
    : a_display_id_(a_display_id),
      b_display_id_(b_display_id),
      a_indicator_bounds_(a_indicator_bounds),
      b_indicator_bounds_(b_indicator_bounds),
      shared_display_edge_indicator_(nullptr) {
  // Initialize edge bounds from indicator bounds.
  aura::Window* a_window = GetRootWindowForDisplayId(a_display_id);
  aura::Window* b_window = GetRootWindowForDisplayId(b_display_id);

  AshWindowTreeHost* a_ash_host = GetRootWindowController(a_window)->ash_host();
  AshWindowTreeHost* b_ash_host = GetRootWindowController(b_window)->ash_host();

  a_edge_bounds_in_native_ =
      GetNativeEdgeBounds(a_ash_host, a_indicator_bounds);
  b_edge_bounds_in_native_ =
      GetNativeEdgeBounds(b_ash_host, b_indicator_bounds);
}

ExtendedMouseWarpController::WarpRegion::~WarpRegion() {}

ExtendedMouseWarpController::ExtendedMouseWarpController(
    aura::Window* drag_source)
    : drag_source_root_(drag_source),
      allow_non_native_event_(false) {
  ash::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();

  // For the time being, 3 or more displays are always always laid out
  // horizontally, with each display being RIGHT of the previous one.
  if (display_manager->GetNumDisplays() > 2) {
    for (size_t i = 1; i < display_manager->GetNumDisplays(); ++i) {
      const gfx::Display& left = display_manager->GetDisplayAt(i - 1);
      const gfx::Display& right = display_manager->GetDisplayAt(i);

      AddWarpRegion(CreateVerticalEdgeBounds(left, right, DisplayLayout::RIGHT),
                    drag_source != nullptr);
    }
  } else {
    // Make sure to set |a| as the primary display, and |b| as the secondary
    // display. DisplayLayout::Position is defined in terms of primary.
    DisplayLayout::Position position =
        display_manager->GetCurrentDisplayLayout().position;
    const gfx::Display& a = Shell::GetScreen()->GetPrimaryDisplay();
    const gfx::Display& b = ScreenUtil::GetSecondaryDisplay();

    // TODO(oshima): Use ComputeBondary instead.
    if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM) {
      AddWarpRegion(CreateHorizontalEdgeBounds(a, b, position),
                    drag_source != nullptr);
    } else {
      AddWarpRegion(CreateVerticalEdgeBounds(a, b, position),
                    drag_source != nullptr);
    }
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

void ExtendedMouseWarpController::AddWarpRegion(
    scoped_ptr<WarpRegion> warp_region,
    bool has_drag_source) {
  if (has_drag_source) {
    warp_region->shared_display_edge_indicator_.reset(
        new SharedDisplayEdgeIndicator);
    warp_region->shared_display_edge_indicator_->Show(
        warp_region->a_indicator_bounds_, warp_region->b_indicator_bounds_);
  }

  warp_regions_.emplace_back(std::move(warp_region));
}

bool ExtendedMouseWarpController::WarpMouseCursorInNativeCoords(
    const gfx::Point& point_in_native,
    const gfx::Point& point_in_screen,
    bool update_mouse_location_now) {
  for (const scoped_ptr<WarpRegion>& warp : warp_regions_) {
    bool in_a_edge = warp->a_edge_bounds_in_native_.Contains(point_in_native);
    bool in_b_edge = warp->b_edge_bounds_in_native_.Contains(point_in_native);
    if (!in_a_edge && !in_b_edge)
      continue;

    // The mouse must move.
    aura::Window* dst_window = GetRootWindowForDisplayId(
        in_a_edge ? warp->b_display_id_ : warp->a_display_id_);
    AshWindowTreeHost* target_ash_host =
        GetRootWindowController(dst_window)->ash_host();

    MoveCursorTo(target_ash_host, point_in_screen, update_mouse_location_now);
    return true;
  }

  return false;
}

scoped_ptr<ExtendedMouseWarpController::WarpRegion>
ExtendedMouseWarpController::CreateHorizontalEdgeBounds(
    const gfx::Display& a,
    const gfx::Display& b,
    DisplayLayout::Position position) {
  bool from_a = a.id() == GetDisplayFromWindow(drag_source_root_).id();

  const gfx::Rect& a_bounds = a.bounds();
  const gfx::Rect& b_bounds = b.bounds();

  gfx::Rect a_indicator_bounds;
  a_indicator_bounds.set_x(std::max(a_bounds.x(), b_bounds.x()));
  a_indicator_bounds.set_width(std::min(a_bounds.right(), b_bounds.right()) -
                               a_indicator_bounds.x());
  a_indicator_bounds.set_height(kIndicatorThickness);
  a_indicator_bounds.set_y(
      position == DisplayLayout::TOP
          ? a_bounds.y() - (from_a ? 0 : kIndicatorThickness)
          : a_bounds.bottom() - (from_a ? kIndicatorThickness : 0));

  gfx::Rect b_indicator_bounds;
  b_indicator_bounds = a_indicator_bounds;
  b_indicator_bounds.set_height(kIndicatorThickness);
  b_indicator_bounds.set_y(
      position == DisplayLayout::TOP
          ? a_bounds.y() - (from_a ? kIndicatorThickness : 0)
          : a_bounds.bottom() - (from_a ? 0 : kIndicatorThickness));

  return make_scoped_ptr(
      new WarpRegion(a.id(), b.id(), a_indicator_bounds, b_indicator_bounds));
}

scoped_ptr<ExtendedMouseWarpController::WarpRegion>
ExtendedMouseWarpController::CreateVerticalEdgeBounds(
    const gfx::Display& a,
    const gfx::Display& b,
    DisplayLayout::Position position) {
  int snap_height = drag_source_root_ ? kMaximumSnapHeight : 0;
  bool in_a = a.id() == GetDisplayFromWindow(drag_source_root_).id();

  const gfx::Rect& a_bounds = a.bounds();
  const gfx::Rect& b_bounds = b.bounds();

  int upper_shared_y = std::max(a_bounds.y(), b_bounds.y());
  int lower_shared_y = std::min(a_bounds.bottom(), b_bounds.bottom());
  int shared_height = lower_shared_y - upper_shared_y;

  gfx::Rect a_indicator_bounds;
  gfx::Rect b_indicator_bounds;

  int dst_x = position == DisplayLayout::LEFT
                  ? a_bounds.x() - (in_a ? kIndicatorThickness : 0)
                  : a_bounds.right() - (in_a ? 0 : kIndicatorThickness);
  b_indicator_bounds.SetRect(dst_x, upper_shared_y, kIndicatorThickness,
                             shared_height);

  // The indicator on the source display.
  a_indicator_bounds.set_width(kIndicatorThickness);
  a_indicator_bounds.set_x(position == DisplayLayout::LEFT
                               ? a_bounds.x() - (in_a ? 0 : kIndicatorThickness)
                               : a_bounds.right() -
                                     (in_a ? kIndicatorThickness : 0));

  const gfx::Rect& source_bounds = in_a ? a_bounds : b_bounds;
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
  a_indicator_bounds.set_y(upper_indicator_y);
  a_indicator_bounds.set_height(lower_indicator_y - upper_indicator_y);

  return make_scoped_ptr(
      new WarpRegion(a.id(), b.id(), a_indicator_bounds, b_indicator_bounds));
}

}  // namespace ash
