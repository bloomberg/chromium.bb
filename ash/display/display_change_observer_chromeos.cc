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
#include "ash/touch/touchscreen_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/events/device_data_manager.h"
#include "ui/events/touchscreen_device.h"
#include "ui/gfx/display.h"
#include "ui/wm/core/user_activity_detector.h"

namespace ash {

using ui::DisplayConfigurator;

namespace {

// The DPI threshold to determine the device scale factor.
// DPI higher than |dpi| will use |device_scale_factor|.
struct DeviceScaleFactorDPIThreshold {
  float dpi;
  float device_scale_factor;
};

const DeviceScaleFactorDPIThreshold kThresholdTable[] = {
  {180.0f, 2.0f},
  {150.0f, 1.25f},
  {0.0f, 1.0f},
};

// 1 inch in mm.
const float kInchInMm = 25.4f;

// The minimum pixel width whose monitor can be called as '4K'.
const int kMinimumWidthFor4K = 3840;

// The list of device scale factors (in addition to 1.0f) which is
// available in extrenal large monitors.
const float kAdditionalDeviceScaleFactorsFor4k[] = {1.25f, 2.0f};

// Display mode list is sorted by:
//  * the area in pixels in ascending order
//  * refresh rate in descending order
struct DisplayModeSorter {
  bool operator()(const DisplayMode& a, const DisplayMode& b) {
    gfx::Size size_a_dip = a.GetSizeInDIP();
    gfx::Size size_b_dip = b.GetSizeInDIP();
    if (size_a_dip.GetArea() == size_b_dip.GetArea())
      return (a.refresh_rate > b.refresh_rate);
    return (size_a_dip.GetArea() < size_b_dip.GetArea());
  }
};

}  // namespace

// static
std::vector<DisplayMode> DisplayChangeObserver::GetInternalDisplayModeList(
    const DisplayInfo& display_info,
    const DisplayConfigurator::DisplayState& output) {
  std::vector<DisplayMode> display_mode_list;
  const ui::DisplayMode* ui_native_mode = output.display->native_mode();
  DisplayMode native_mode(ui_native_mode->size(),
                          ui_native_mode->refresh_rate(),
                          ui_native_mode->is_interlaced(),
                          true);
  native_mode.device_scale_factor = display_info.device_scale_factor();
  std::vector<float> ui_scales =
      DisplayManager::GetScalesForDisplay(display_info);
  float native_ui_scale = (display_info.device_scale_factor() == 1.25f) ?
      1.0f : display_info.device_scale_factor();
  for (size_t i = 0; i < ui_scales.size(); ++i) {
    DisplayMode mode = native_mode;
    mode.ui_scale = ui_scales[i];
    mode.native = (ui_scales[i] == native_ui_scale);
    display_mode_list.push_back(mode);
  }

  std::sort(
      display_mode_list.begin(), display_mode_list.end(), DisplayModeSorter());
  return display_mode_list;
}

// static
std::vector<DisplayMode> DisplayChangeObserver::GetExternalDisplayModeList(
    const DisplayConfigurator::DisplayState& output) {
  typedef std::map<std::pair<int, int>, DisplayMode> DisplayModeMap;
  DisplayModeMap display_mode_map;

  DisplayMode native_mode;
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
    if (display_mode.native)
      native_mode = display_mode;

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

  if (native_mode.size.width() >= kMinimumWidthFor4K) {
    for (size_t i = 0; i < arraysize(kAdditionalDeviceScaleFactorsFor4k);
         ++i) {
      DisplayMode mode = native_mode;
      mode.device_scale_factor = kAdditionalDeviceScaleFactorsFor4k[i];
      mode.native = false;
      display_mode_list.push_back(mode);
    }
  }

  std::sort(
      display_mode_list.begin(), display_mode_list.end(), DisplayModeSorter());
  return display_mode_list;
}

DisplayChangeObserver::DisplayChangeObserver() {
  Shell::GetInstance()->AddShellObserver(this);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

DisplayChangeObserver::~DisplayChangeObserver() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
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
    if (state.display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!ui::IsDisplaySizeBlackListed(state.display->physical_size())) {
        device_scale_factor =
            FindDeviceScaleFactor((kInchInMm * mode_info->size().width() /
                                   state.display->physical_size().width()));
      }
    } else {
      DisplayMode mode;
      if (Shell::GetInstance()->display_manager()->GetSelectedModeForDisplayId(
              state.display->display_id(), &mode)) {
        device_scale_factor = mode.device_scale_factor;
      }
    }
    gfx::Rect display_bounds(state.display->origin(), mode_info->size());

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
    new_info.set_is_aspect_preserving_scaling(
        state.display->is_aspect_preserving_scaling());

    std::vector<DisplayMode> display_modes =
        (state.display->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) ?
        GetInternalDisplayModeList(new_info, state) :
        GetExternalDisplayModeList(state);
    new_info.set_display_modes(display_modes);

    new_info.set_available_color_profiles(
        Shell::GetInstance()
            ->display_configurator()
            ->GetAvailableColorCalibrationProfiles(id));
  }

  AssociateTouchscreens(
      &displays, ui::DeviceDataManager::GetInstance()->touchscreen_devices());
  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);

  // For the purposes of user activity detection, ignore synthetic mouse events
  // that are triggered by screen resizes: http://crbug.com/360634
  ::wm::UserActivityDetector* user_activity_detector =
      Shell::GetInstance()->user_activity_detector();
  if (user_activity_detector)
    user_activity_detector->OnDisplayPowerChanging();
}

void DisplayChangeObserver::OnAppTerminating() {
#if defined(USE_ASH)
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  Shell::GetInstance()->display_configurator()->PrepareForExit();
#endif
}

// static
float DisplayChangeObserver::FindDeviceScaleFactor(float dpi) {
  for (size_t i = 0; i < arraysize(kThresholdTable); ++i) {
    if (dpi > kThresholdTable[i].dpi)
      return kThresholdTable[i].device_scale_factor;
  }
  return 1.0f;
}

void DisplayChangeObserver::OnInputDeviceConfigurationChanged() {
  std::vector<DisplayInfo> display_infos;
  DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  const std::vector<gfx::Display>& displays = display_manager->displays();
  // Reuse the current state in DisplayManager and re-associate the displays
  // with the touchscreens.
  for (size_t i = 0; i < displays.size(); ++i) {
    DisplayInfo display = display_manager->GetDisplayInfo(displays[i].id());
    // Unset the touchscreen configuration since we'll be rematching them from
    // scratch.
    display.set_touch_device_id(ui::TouchscreenDevice::kInvalidId);
    display.set_touch_support(gfx::Display::TOUCH_SUPPORT_UNKNOWN);

    display_infos.push_back(display);
  }

  AssociateTouchscreens(
      &display_infos,
      ui::DeviceDataManager::GetInstance()->touchscreen_devices());
  display_manager->OnNativeDisplaysChanged(display_infos);
}

}  // namespace ash
