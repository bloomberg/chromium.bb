// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include <algorithm>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#endif

namespace ash {
namespace {

// List of value UI Scale values. Scales for 2x are equivalent to 640,
// 800, 1024, 1280, 1440, 1600 and 1920 pixel width respectively on
// 2560 pixel width 2x density display. Please see crbug.com/233375
// for the full list of resolutions.
const float kUIScalesFor2x[] =
    {0.5f, 0.625f, 0.8f, 1.0f, 1.125f, 1.25f, 1.5f, 2.0f};
const float kUIScalesFor1_25x[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.25f };
const float kUIScalesFor1280[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f };
const float kUIScalesFor1366[] = {0.5f, 0.6f, 0.75f, 1.0f, 1.125f };

std::vector<float> GetScalesForDisplay(const DisplayMode& native_mode) {
#define ASSIGN_ARRAY(v, a) v.assign(a, a + arraysize(a))

  std::vector<float> ret;
  if (native_mode.device_scale_factor == 2.0f) {
    ASSIGN_ARRAY(ret, kUIScalesFor2x);
    return ret;
  } else if (native_mode.device_scale_factor == 1.25f) {
    ASSIGN_ARRAY(ret, kUIScalesFor1_25x);
    return ret;
  }
  switch (native_mode.size.width()) {
    case 1280:
      ASSIGN_ARRAY(ret, kUIScalesFor1280);
      break;
    case 1366:
      ASSIGN_ARRAY(ret, kUIScalesFor1366);
      break;
    default:
      ASSIGN_ARRAY(ret, kUIScalesFor1280);
#if defined(OS_CHROMEOS)
      if (base::SysInfo::IsRunningOnChromeOS())
        NOTREACHED() << "Unknown resolution:" << native_mode.size.ToString();
#endif
  }
  return ret;
}

struct ScaleComparator {
  explicit ScaleComparator(float s) : scale(s) {}

  bool operator()(const DisplayMode& mode) const {
    const float kEpsilon = 0.0001f;
    return std::abs(scale - mode.ui_scale) < kEpsilon;
  }
  float scale;
};

void ConvertPointFromScreenToNative(aura::WindowTreeHost* host,
                                    gfx::Point* point) {
  ::wm::ConvertPointFromScreen(host->window(), point);
  host->ConvertPointToNativeScreen(point);
}

}  // namespace

std::vector<DisplayMode> CreateInternalDisplayModeList(
    const DisplayMode& native_mode) {
  std::vector<DisplayMode> display_mode_list;

  float native_ui_scale = (native_mode.device_scale_factor == 1.25f)
                              ? 1.0f
                              : native_mode.device_scale_factor;
  for (float ui_scale : GetScalesForDisplay(native_mode)) {
    DisplayMode mode = native_mode;
    mode.ui_scale = ui_scale;
    mode.native = (ui_scale == native_ui_scale);
    display_mode_list.push_back(mode);
  }
  return display_mode_list;
}

// static
float GetNextUIScale(const DisplayInfo& info, bool up) {
  ScaleComparator comparator(info.configured_ui_scale());
  const std::vector<DisplayMode>& modes = info.display_modes();
  for (auto iter = modes.begin(); iter != modes.end(); ++iter) {
    if (comparator(*iter)) {
      if (up && (iter + 1) != modes.end())
        return (iter + 1)->ui_scale;
      if (!up && iter != modes.begin())
        return (iter - 1)->ui_scale;
      return info.configured_ui_scale();
    }
  }
  // Fallback to 1.0f if the |scale| wasn't in the list.
  return 1.0f;
}

bool HasDisplayModeForUIScale(const DisplayInfo& info, float ui_scale) {
  ScaleComparator comparator(ui_scale);
  const std::vector<DisplayMode>& modes = info.display_modes();
  return std::find_if(modes.begin(), modes.end(), comparator) != modes.end();
}

void ComputeBoundary(const gfx::Display& primary_display,
                     const gfx::Display& secondary_display,
                     DisplayLayout::Position position,
                     gfx::Rect* primary_edge_in_screen,
                     gfx::Rect* secondary_edge_in_screen) {
  const gfx::Rect& primary = primary_display.bounds();
  const gfx::Rect& secondary = secondary_display.bounds();
  switch (position) {
    case DisplayLayout::TOP:
    case DisplayLayout::BOTTOM: {
      int left = std::max(primary.x(), secondary.x());
      int right = std::min(primary.right(), secondary.right());
      if (position == DisplayLayout::TOP) {
        primary_edge_in_screen->SetRect(left, primary.y(), right - left, 1);
        secondary_edge_in_screen->SetRect(left, secondary.bottom() - 1,
                                          right - left, 1);
      } else {
        primary_edge_in_screen->SetRect(left, primary.bottom() - 1,
                                        right - left, 1);
        secondary_edge_in_screen->SetRect(left, secondary.y(), right - left, 1);
      }
      break;
    }
    case DisplayLayout::LEFT:
    case DisplayLayout::RIGHT: {
      int top = std::max(primary.y(), secondary.y());
      int bottom = std::min(primary.bottom(), secondary.bottom());
      if (position == DisplayLayout::LEFT) {
        primary_edge_in_screen->SetRect(primary.x(), top, 1, bottom - top);
        secondary_edge_in_screen->SetRect(secondary.right() - 1, top, 1,
                                          bottom - top);
      } else {
        primary_edge_in_screen->SetRect(primary.right() - 1, top, 1,
                                        bottom - top);
        secondary_edge_in_screen->SetRect(secondary.y(), top, 1, bottom - top);
      }
      break;
    }
  }
}

gfx::Rect GetNativeEdgeBounds(AshWindowTreeHost* ash_host,
                              const gfx::Rect& bounds_in_screen) {
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  gfx::Rect native_bounds = host->GetBounds();
  native_bounds.Inset(ash_host->GetHostInsets());
  gfx::Point start_in_native = bounds_in_screen.origin();
  gfx::Point end_in_native = bounds_in_screen.bottom_right();

  ConvertPointFromScreenToNative(host, &start_in_native);
  ConvertPointFromScreenToNative(host, &end_in_native);

  if (std::abs(start_in_native.x() - end_in_native.x()) <
      std::abs(start_in_native.y() - end_in_native.y())) {
    // vertical in native
    int x = std::abs(native_bounds.x() - start_in_native.x()) <
                    std::abs(native_bounds.right() - start_in_native.x())
                ? native_bounds.x()
                : native_bounds.right() - 1;
    return gfx::Rect(x, std::min(start_in_native.y(), end_in_native.y()), 1,
                     std::abs(end_in_native.y() - start_in_native.y()));
  } else {
    // horizontal in native
    int y = std::abs(native_bounds.y() - start_in_native.y()) <
                    std::abs(native_bounds.bottom() - start_in_native.y())
                ? native_bounds.y()
                : native_bounds.bottom() - 1;
    return gfx::Rect(std::min(start_in_native.x(), end_in_native.x()), y,
                     std::abs(end_in_native.x() - start_in_native.x()), 1);
  }
}

// Moves the cursor to the point inside the root that is closest to
// the point_in_screen, which is outside of the root window.
void MoveCursorTo(AshWindowTreeHost* ash_host,
                  const gfx::Point& point_in_screen,
                  bool update_last_location_now) {
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  gfx::Point point_in_native = point_in_screen;
  ::wm::ConvertPointFromScreen(host->window(), &point_in_native);
  host->ConvertPointToNativeScreen(&point_in_native);

  // now fit the point inside the native bounds.
  gfx::Rect native_bounds = host->GetBounds();
  gfx::Point native_origin = native_bounds.origin();
  native_bounds.Inset(ash_host->GetHostInsets());
  // Shrink further so that the mouse doesn't warp on the
  // edge. The right/bottom needs to be shrink by 2 to subtract
  // the 1 px from width/height value.
  native_bounds.Inset(1, 1, 2, 2);

  // Ensure that |point_in_native| is inside the |native_bounds|.
  point_in_native.SetToMax(native_bounds.origin());
  point_in_native.SetToMin(native_bounds.bottom_right());

  gfx::Point point_in_host = point_in_native;

  point_in_host.Offset(-native_origin.x(), -native_origin.y());
  host->MoveCursorToHostLocation(point_in_host);

  if (update_last_location_now) {
    gfx::Point new_point_in_screen = point_in_native;
    if (Shell::GetInstance()->display_manager()->IsInUnifiedMode()) {
      // TODO(oshima): Do not use ConvertPointFromNativeScreen because
      // the mirroring display has a transform that should not be applied here.
      gfx::Point origin = host->GetBounds().origin();
      new_point_in_screen.Offset(-origin.x(), -origin.y());
    } else {
      host->ConvertPointFromNativeScreen(&new_point_in_screen);
    }
    ::wm::ConvertPointToScreen(host->window(), &new_point_in_screen);
    aura::Env::GetInstance()->set_last_mouse_location(new_point_in_screen);
  }
}

int FindDisplayIndexContainingPoint(const std::vector<gfx::Display>& displays,
                                    const gfx::Point& point_in_screen) {
  auto iter = std::find_if(displays.begin(), displays.end(),
                           [point_in_screen](const gfx::Display& display) {
                             return display.bounds().Contains(point_in_screen);
                           });
  return iter == displays.end() ? -1 : (iter - displays.begin());
}

}  // namespace ash
