// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include <algorithm>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
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

bool GetDisplayModeForUIScale(const DisplayInfo& info,
                              float ui_scale,
                              DisplayMode* out) {
  const std::vector<DisplayMode>& modes = info.display_modes();
  auto iter = std::find_if(modes.begin(), modes.end(),
                           [ui_scale](const DisplayMode& mode) {
                             return mode.ui_scale == ui_scale;
                           });
  if (iter == modes.end())
    return false;
  *out = *iter;
  return true;
}

void FindNextMode(std::vector<DisplayMode>::const_iterator& iter,
                  const std::vector<DisplayMode>& modes,
                  bool up,
                  DisplayMode* out) {
  DCHECK(iter != modes.end());
  if (up && (iter + 1) != modes.end())
    *out = *(iter + 1);
  else if (!up && iter != modes.begin())
    *out = *(iter - 1);
  else
    *out = *iter;
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

std::vector<DisplayMode> CreateUnifiedDisplayModeList(
    const DisplayMode& native_mode,
    const std::set<std::pair<float, float>>& dsf_scale_list) {
  std::vector<DisplayMode> display_mode_list;

  for (auto& pair : dsf_scale_list) {
    DisplayMode mode = native_mode;
    mode.device_scale_factor = pair.first;
    gfx::SizeF scaled_size(native_mode.size);
    scaled_size.Scale(pair.second);
    mode.size = gfx::ToFlooredSize(scaled_size);
    mode.native = false;
    display_mode_list.push_back(mode);
  }
  // Sort the mode by the size in DIP.
  std::sort(display_mode_list.begin(), display_mode_list.end(),
            [](const DisplayMode& a, const DisplayMode& b) {
              return a.GetSizeInDIP(false).GetArea() <
                     b.GetSizeInDIP(false).GetArea();
            });
  return display_mode_list;
}

bool GetDisplayModeForResolution(const DisplayInfo& info,
                                 const gfx::Size& resolution,
                                 DisplayMode* out) {
  if (display::Display::IsInternalDisplayId(info.id()))
    return false;

  const std::vector<DisplayMode>& modes = info.display_modes();
  DCHECK_NE(0u, modes.size());
  DisplayMode target_mode;
  target_mode.size = resolution;
  std::vector<DisplayMode>::const_iterator iter = std::find_if(
      modes.begin(), modes.end(), [resolution](const DisplayMode& mode) {
        return mode.size == resolution;
      });
  if (iter == modes.end()) {
    LOG(WARNING) << "Unsupported resolution was requested:"
                 << resolution.ToString();
    return false;
  }
  *out = *iter;
  return true;
}

bool GetDisplayModeForNextUIScale(const DisplayInfo& info,
                                  bool up,
                                  DisplayMode* out) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->IsActiveDisplayId(info.id()) ||
      !display::Display::IsInternalDisplayId(info.id())) {
    return false;
  }
  const std::vector<DisplayMode>& modes = info.display_modes();
  ScaleComparator comparator(info.configured_ui_scale());
  auto iter = std::find_if(modes.begin(), modes.end(), comparator);
  FindNextMode(iter, modes, up, out);
  return true;
}

bool GetDisplayModeForNextResolution(const DisplayInfo& info,
                                     bool up,
                                     DisplayMode* out) {
  if (display::Display::IsInternalDisplayId(info.id()))
    return false;
  const std::vector<DisplayMode>& modes = info.display_modes();
  DisplayMode tmp(info.size_in_pixel(), 0.0f, false, false);
  tmp.device_scale_factor = info.device_scale_factor();
  gfx::Size resolution = tmp.GetSizeInDIP(false);
  auto iter = std::find_if(modes.begin(), modes.end(),
                           [resolution](const DisplayMode& mode) {
                             return mode.GetSizeInDIP(false) == resolution;
                           });
  FindNextMode(iter, modes, up, out);
  return true;
}

bool SetDisplayUIScale(int64_t id, float ui_scale) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->IsActiveDisplayId(id) ||
      !display::Display::IsInternalDisplayId(id)) {
    return false;
  }
  const DisplayInfo& info = display_manager->GetDisplayInfo(id);
  DisplayMode mode;
  if (!GetDisplayModeForUIScale(info, ui_scale, &mode))
    return false;
  return display_manager->SetDisplayMode(id, mode);
}

bool HasDisplayModeForUIScale(const DisplayInfo& info, float ui_scale) {
  ScaleComparator comparator(ui_scale);
  const std::vector<DisplayMode>& modes = info.display_modes();
  return std::find_if(modes.begin(), modes.end(), comparator) != modes.end();
}

bool ComputeBoundary(const display::Display& a_display,
                     const display::Display& b_display,
                     gfx::Rect* a_edge_in_screen,
                     gfx::Rect* b_edge_in_screen) {
  const gfx::Rect& a_bounds = a_display.bounds();
  const gfx::Rect& b_bounds = b_display.bounds();

  // Find touching side.
  int rx = std::max(a_bounds.x(), b_bounds.x());
  int ry = std::max(a_bounds.y(), b_bounds.y());
  int rr = std::min(a_bounds.right(), b_bounds.right());
  int rb = std::min(a_bounds.bottom(), b_bounds.bottom());

  display::DisplayPlacement::Position position;
  if ((rb - ry) == 0) {
    // top bottom
    if (a_bounds.bottom() == b_bounds.y()) {
      position = display::DisplayPlacement::BOTTOM;
    } else if (a_bounds.y() == b_bounds.bottom()) {
      position = display::DisplayPlacement::TOP;
    } else {
      return false;
    }
  } else {
    // left right
    if (a_bounds.right() == b_bounds.x()) {
      position = display::DisplayPlacement::RIGHT;
    } else if (a_bounds.x() == b_bounds.right()) {
      position = display::DisplayPlacement::LEFT;
    } else {
      DCHECK_NE(rr, rx);
      return false;
    }
  }

  switch (position) {
    case display::DisplayPlacement::TOP:
    case display::DisplayPlacement::BOTTOM: {
      int left = std::max(a_bounds.x(), b_bounds.x());
      int right = std::min(a_bounds.right(), b_bounds.right());
      if (position == display::DisplayPlacement::TOP) {
        a_edge_in_screen->SetRect(left, a_bounds.y(), right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.bottom() - 1, right - left, 1);
      } else {
        a_edge_in_screen->SetRect(left, a_bounds.bottom() - 1, right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.y(), right - left, 1);
      }
      break;
    }
    case display::DisplayPlacement::LEFT:
    case display::DisplayPlacement::RIGHT: {
      int top = std::max(a_bounds.y(), b_bounds.y());
      int bottom = std::min(a_bounds.bottom(), b_bounds.bottom());
      if (position == display::DisplayPlacement::LEFT) {
        a_edge_in_screen->SetRect(a_bounds.x(), top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.right() - 1, top, 1, bottom - top);
      } else {
        a_edge_in_screen->SetRect(a_bounds.right() - 1, top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.x(), top, 1, bottom - top);
      }
      break;
    }
  }
  return true;
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
    gfx::Point new_point_in_screen;
    if (Shell::GetInstance()->display_manager()->IsInUnifiedMode()) {
      new_point_in_screen = point_in_host;
      // First convert to the unified host.
      host->ConvertPointFromHost(&new_point_in_screen);
      // Then convert to the unified screen.
      Shell::GetPrimaryRootWindow()->GetHost()->ConvertPointFromHost(
          &new_point_in_screen);
    } else {
      new_point_in_screen = point_in_native;
      host->ConvertPointFromNativeScreen(&new_point_in_screen);
      ::wm::ConvertPointToScreen(host->window(), &new_point_in_screen);
    }
    aura::Env::GetInstance()->set_last_mouse_location(new_point_in_screen);
  }
}

int FindDisplayIndexContainingPoint(
    const std::vector<display::Display>& displays,
    const gfx::Point& point_in_screen) {
  auto iter = std::find_if(displays.begin(), displays.end(),
                           [point_in_screen](const display::Display& display) {
                             return display.bounds().Contains(point_in_screen);
                           });
  return iter == displays.end() ? -1 : (iter - displays.begin());
}

display::DisplayIdList CreateDisplayIdList(const display::DisplayList& list) {
  return GenerateDisplayIdList(
      list.begin(), list.end(),
      [](const display::Display& display) { return display.id(); });
}

void SortDisplayIdList(display::DisplayIdList* ids) {
  std::sort(ids->begin(), ids->end(),
            [](int64_t a, int64_t b) { return CompareDisplayIds(a, b); });
}

std::string DisplayIdListToString(const display::DisplayIdList& list) {
  std::stringstream s;
  const char* sep = "";
  for (int64_t id : list) {
    s << sep << id;
    sep = ",";
  }
  return s.str();
}

bool CompareDisplayIds(int64_t id1, int64_t id2) {
  DCHECK_NE(id1, id2);
  // Output index is stored in the first 8 bits. See GetDisplayIdFromEDID
  // in edid_parser.cc.
  int index_1 = id1 & 0xFF;
  int index_2 = id2 & 0xFF;
  DCHECK_NE(index_1, index_2) << id1 << " and " << id2;
  return display::Display::IsInternalDisplayId(id1) ||
         (index_1 < index_2 && !display::Display::IsInternalDisplayId(id2));
}

}  // namespace ash
