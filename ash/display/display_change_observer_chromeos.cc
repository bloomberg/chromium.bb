// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_chromeos.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/display/output_util.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"

namespace ash {
namespace internal {

using chromeos::OutputConfigurator;

namespace {

// The DPI threshold to detect high density screen.
// Higher DPI than this will use device_scale_factor=2.
const unsigned int kHighDensityDPIThreshold = 170;

// 1 inch in mm.
const float kInchInMm = 25.4f;

// Display mode list is sorted by (in descending priority):
//  * the area in pixels.
//  * refresh rate.
struct DisplayModeSorter {
  bool operator()(const DisplayMode& a, const DisplayMode& b) {
    if (a.size.GetArea() == b.size.GetArea())
      return (a.refresh_rate > b.refresh_rate);
    return (a.size.GetArea() > b.size.GetArea());
  }
};

}  // namespace

// static
std::vector<DisplayMode> DisplayChangeObserver::GetDisplayModeList(
    const OutputConfigurator::OutputSnapshot& output) {
  typedef std::map<std::pair<int, int>, DisplayMode> DisplayModeMap;
  DisplayModeMap display_mode_map;

  for (std::map<RRMode, OutputConfigurator::ModeInfo>::const_iterator it =
       output.mode_infos.begin(); it != output.mode_infos.end(); ++it) {
    const OutputConfigurator::ModeInfo& mode_info = it->second;
    const std::pair<int, int> size(mode_info.width, mode_info.height);
    const DisplayMode display_mode(gfx::Size(mode_info.width, mode_info.height),
                                   mode_info.refresh_rate,
                                   mode_info.interlaced,
                                   output.native_mode == it->first);

    // Add the display mode if it isn't already present and override interlaced
    // display modes with non-interlaced ones.
    DisplayModeMap::iterator display_mode_it = display_mode_map.find(size);
    if (display_mode_it == display_mode_map.end())
      display_mode_map.insert(std::make_pair(size, display_mode));
    else if (display_mode_it->second.interlaced && !display_mode.interlaced)
      display_mode_it->second = display_mode;
  }

  std::vector<DisplayMode> display_mode_list;
  for (DisplayModeMap::const_iterator iter = display_mode_map.begin();
       iter != display_mode_map.end();
       ++iter) {
    display_mode_list.push_back(iter->second);
  }
  std::sort(
      display_mode_list.begin(), display_mode_list.end(), DisplayModeSorter());
  return display_mode_list;
}

DisplayChangeObserver::DisplayChangeObserver() {
  Shell::GetInstance()->AddShellObserver(this);
}

DisplayChangeObserver::~DisplayChangeObserver() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

ui::OutputState DisplayChangeObserver::GetStateForDisplayIds(
    const std::vector<int64>& display_ids) const {
  CHECK_EQ(2U, display_ids.size());
  DisplayIdPair pair = std::make_pair(display_ids[0], display_ids[1]);
  DisplayLayout layout = Shell::GetInstance()->display_manager()->
      layout_store()->GetRegisteredDisplayLayout(pair);
  return layout.mirrored ? ui::OUTPUT_STATE_DUAL_MIRROR :
                           ui::OUTPUT_STATE_DUAL_EXTENDED;
}

bool DisplayChangeObserver::GetResolutionForDisplayId(int64 display_id,
                                                      int* width,
                                                      int* height) const {
  DisplayMode mode;
  if (!Shell::GetInstance()->display_manager()->GetSelectedModeForDisplayId(
           display_id, &mode))
    return false;

  *width = mode.size.width();
  *height = mode.size.height();
  return true;
}

void DisplayChangeObserver::OnDisplayModeChanged(
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  std::vector<DisplayInfo> displays;
  std::set<int64> ids;
  for (size_t i = 0; i < outputs.size(); ++i) {
    const OutputConfigurator::OutputSnapshot& output = outputs[i];

    if (output.type == ui::OUTPUT_TYPE_INTERNAL &&
        gfx::Display::InternalDisplayId() == gfx::Display::kInvalidDisplayID) {
      // Fall back to output index. crbug.com/180100
      gfx::Display::SetInternalDisplayId(
          output.display_id == gfx::Display::kInvalidDisplayID ? output.index :
          output.display_id);
    }

    const OutputConfigurator::ModeInfo* mode_info =
        OutputConfigurator::GetModeInfo(output, output.current_mode);
    if (!mode_info)
      continue;

    float device_scale_factor = 1.0f;
    if (!ui::IsXDisplaySizeBlackListed(output.width_mm, output.height_mm) &&
        (kInchInMm * mode_info->width / output.width_mm) >
        kHighDensityDPIThreshold) {
      device_scale_factor = 2.0f;
    }
    gfx::Rect display_bounds(
        output.x, output.y, mode_info->width, mode_info->height);

    std::vector<DisplayMode> display_modes = GetDisplayModeList(output);

    std::string name =
        output.type == ui::OUTPUT_TYPE_INTERNAL
            ? l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME)
            : chromeos::GetDisplayName(output.output);
    if (name.empty())
      name = l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

    bool has_overscan = false;
    chromeos::GetOutputOverscanFlag(output.output, &has_overscan);

    int64 id = output.display_id;
    if (id == gfx::Display::kInvalidDisplayID || ids.find(id) != ids.end())
      id = output.index;
    ids.insert(id);

    displays.push_back(DisplayInfo(id, name, has_overscan));
    displays.back().set_device_scale_factor(device_scale_factor);
    displays.back().SetBounds(display_bounds);
    displays.back().set_native(true);
    displays.back().set_display_modes(display_modes);
    displays.back().set_touch_support(
        output.touch_device_id == 0 ? gfx::Display::TOUCH_SUPPORT_UNAVAILABLE :
                                      gfx::Display::TOUCH_SUPPORT_AVAILABLE);
  }

  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);
}

void DisplayChangeObserver::OnAppTerminating() {
#if defined(USE_ASH)
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  Shell::GetInstance()->output_configurator()->Stop();
#endif
}

}  // namespace internal
}  // namespace ash
