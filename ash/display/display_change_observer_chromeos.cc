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
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/types/chromeos/display_mode.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/display.h"

namespace ash {

using ui::DisplayConfigurator;

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
    const DisplayConfigurator::DisplayState& output) {
  typedef std::map<std::pair<int, int>, DisplayMode> DisplayModeMap;
  DisplayModeMap display_mode_map;

  for (std::vector<const ui::DisplayMode*>::const_iterator it =
           output.display->modes().begin();
       it != output.display->modes().end();
       ++it) {
    const ui::DisplayMode& mode_info = **it;
    const std::pair<int, int> size(mode_info.size().width(),
                                   mode_info.size().height());
    const DisplayMode display_mode(mode_info.size(),
                                   mode_info.refresh_rate(),
                                   mode_info.is_interlaced(),
                                   output.display->native_mode() == *it);

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

ui::MultipleDisplayState DisplayChangeObserver::GetStateForDisplayIds(
    const std::vector<int64>& display_ids) const {
  CHECK_EQ(2U, display_ids.size());
  DisplayIdPair pair = std::make_pair(display_ids[0], display_ids[1]);
  DisplayLayout layout = Shell::GetInstance()->display_manager()->
      layout_store()->GetRegisteredDisplayLayout(pair);
  return layout.mirrored ? ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR :
                           ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
}

bool DisplayChangeObserver::GetResolutionForDisplayId(int64 display_id,
                                                      gfx::Size* size) const {
  DisplayMode mode;
  if (!Shell::GetInstance()->display_manager()->GetSelectedModeForDisplayId(
           display_id, &mode))
    return false;

  *size = mode.size;
  return true;
}

void DisplayChangeObserver::OnDisplayModeChanged(
    const std::vector<DisplayConfigurator::DisplayState>& display_states) {
  std::vector<DisplayInfo> displays;
  std::set<int64> ids;
  for (size_t i = 0; i < display_states.size(); ++i) {
    const DisplayConfigurator::DisplayState& state = display_states[i];

    if (state.display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL &&
        gfx::Display::InternalDisplayId() == gfx::Display::kInvalidDisplayID) {
      gfx::Display::SetInternalDisplayId(state.display->display_id());
    }

    const ui::DisplayMode* mode_info = state.display->current_mode();
    if (!mode_info)
      continue;

    float device_scale_factor = 1.0f;
    if (!ui::IsDisplaySizeBlackListed(state.display->physical_size()) &&
        (kInchInMm * mode_info->size().width() /
         state.display->physical_size().width()) > kHighDensityDPIThreshold) {
      device_scale_factor = 2.0f;
    }
    gfx::Rect display_bounds(state.display->origin(), mode_info->size());

    std::vector<DisplayMode> display_modes = GetDisplayModeList(state);

    std::string name =
        state.display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL ?
            l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME) :
            state.display->display_name();
    if (name.empty())
      name = l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

    bool has_overscan = state.display->has_overscan();
    int64 id = state.display->display_id();
    ids.insert(id);

    displays.push_back(DisplayInfo(id, name, has_overscan));
    DisplayInfo& new_info = displays.back();
    new_info.set_device_scale_factor(device_scale_factor);
    new_info.SetBounds(display_bounds);
    new_info.set_native(true);
    new_info.set_display_modes(display_modes);
    new_info.set_touch_support(state.touch_device_id == 0 ?
        gfx::Display::TOUCH_SUPPORT_UNAVAILABLE :
        gfx::Display::TOUCH_SUPPORT_AVAILABLE);
    new_info.set_touch_device_id(state.touch_device_id);
    new_info.set_is_aspect_preserving_scaling(
        state.display->is_aspect_preserving_scaling());
    new_info.set_available_color_profiles(
        Shell::GetInstance()
            ->display_configurator()
            ->GetAvailableColorCalibrationProfiles(id));
  }

  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);
}

void DisplayChangeObserver::OnAppTerminating() {
#if defined(USE_ASH)
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  Shell::GetInstance()->display_configurator()->PrepareForExit();
#endif
}

}  // namespace ash
