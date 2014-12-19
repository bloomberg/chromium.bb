// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include <algorithm>

#include "ash/display/display_info.h"

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

}  // namespace ash
