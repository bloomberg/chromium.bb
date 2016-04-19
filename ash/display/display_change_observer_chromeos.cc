// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_chromeos.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/shell.h"
#include "ash/touch/touchscreen_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/display.h"

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
  {200.0f, 2.0f},
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

void UpdateInternalDisplayId(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  for (auto* state : display_states) {
    if (state->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (gfx::Display::HasInternalDisplay())
        DCHECK_EQ(gfx::Display::InternalDisplayId(), state->display_id());
      gfx::Display::SetInternalDisplayId(state->display_id());
    }
  }
}

}  // namespace

// static
std::vector<DisplayMode> DisplayChangeObserver::GetInternalDisplayModeList(
    const DisplayInfo& display_info,
    const ui::DisplaySnapshot& output) {
  const ui::DisplayMode* ui_native_mode = output.native_mode();
  DisplayMode native_mode(ui_native_mode->size(),
                          ui_native_mode->refresh_rate(),
                          ui_native_mode->is_interlaced(),
                          true);
  native_mode.device_scale_factor = display_info.device_scale_factor();

  return CreateInternalDisplayModeList(native_mode);
}

// static
std::vector<DisplayMode> DisplayChangeObserver::GetExternalDisplayModeList(
    const ui::DisplaySnapshot& output) {
  typedef std::map<std::pair<int, int>, DisplayMode> DisplayModeMap;
  DisplayModeMap display_mode_map;

  DisplayMode native_mode;
  for (const ui::DisplayMode* mode_info : output.modes()) {
    const std::pair<int, int> size(mode_info->size().width(),
                                   mode_info->size().height());
    const DisplayMode display_mode(mode_info->size(), mode_info->refresh_rate(),
                                   mode_info->is_interlaced(),
                                   output.native_mode() == mode_info);
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
  for (const auto& display_mode_pair : display_mode_map)
    display_mode_list.push_back(display_mode_pair.second);

  if (output.native_mode()) {
    const std::pair<int, int> size(native_mode.size.width(),
                                   native_mode.size.height());
    DisplayModeMap::iterator it = display_mode_map.find(size);
    DCHECK(it != display_mode_map.end())
        << "Native mode must be part of the mode list.";

    // If the native mode was replaced re-add it.
    if (!it->second.native)
      display_mode_list.push_back(native_mode);
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
    const ui::DisplayConfigurator::DisplayStateList& display_states) const {
  UpdateInternalDisplayId(display_states);
  if (display_states.size() == 1)
    return ui::MULTIPLE_DISPLAY_STATE_SINGLE;
  display::DisplayIdList list =
      GenerateDisplayIdList(display_states.begin(), display_states.end(),
                            [](const ui::DisplaySnapshot* display_state) {
                              return display_state->display_id();
                            });

  const display::DisplayLayout& layout = Shell::GetInstance()
                                             ->display_manager()
                                             ->layout_store()
                                             ->GetRegisteredDisplayLayout(list);
  return layout.mirrored ? ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR :
                           ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
}

bool DisplayChangeObserver::GetResolutionForDisplayId(int64_t display_id,
                                                      gfx::Size* size) const {
  DisplayMode mode;
  if (!Shell::GetInstance()->display_manager()->GetSelectedModeForDisplayId(
           display_id, &mode))
    return false;

  *size = mode.size;
  return true;
}

void DisplayChangeObserver::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  UpdateInternalDisplayId(display_states);

  std::vector<DisplayInfo> displays;
  std::set<int64_t> ids;
  for (const ui::DisplaySnapshot* state : display_states) {
    const ui::DisplayMode* mode_info = state->current_mode();
    if (!mode_info)
      continue;

    float device_scale_factor = 1.0f;
    // Sets dpi only if the screen size is not blacklisted.
    float dpi = ui::IsDisplaySizeBlackListed(state->physical_size())
                    ? 0
                    : kInchInMm * mode_info->size().width() /
                          state->physical_size().width();
    if (state->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (dpi)
        device_scale_factor = FindDeviceScaleFactor(dpi);
    } else {
      DisplayMode mode;
      if (Shell::GetInstance()->display_manager()->GetSelectedModeForDisplayId(
              state->display_id(), &mode)) {
        device_scale_factor = mode.device_scale_factor;
      } else {
        // For monitors that are 40 inches and 4K or above, set
        // |device_scale_factor| to 2x. For margin purposes, 100 is subtracted
        // from the value of |k2xThreshouldSizeSquaredFor4KInMm|
        const int k2xThreshouldSizeSquaredFor4KInMm =
            (40 * 40 * kInchInMm * kInchInMm) - 100;
        gfx::Vector2d size_in_vec(state->physical_size().width(),
                                  state->physical_size().height());
        if (size_in_vec.LengthSquared() > k2xThreshouldSizeSquaredFor4KInMm &&
            mode_info->size().width() >= kMinimumWidthFor4K) {
          // Make sure that additional device scale factors table has 2x.
          DCHECK_EQ(2.0f, kAdditionalDeviceScaleFactorsFor4k[1]);
          device_scale_factor = 2.0f;
        }
      }
    }
    gfx::Rect display_bounds(state->origin(), mode_info->size());

    std::string name;
    switch (state->type()) {
      case ui::DISPLAY_CONNECTION_TYPE_INTERNAL:
        name = l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME);
        break;
      case ui::DISPLAY_CONNECTION_TYPE_VIRTUAL:
        name = l10n_util::GetStringUTF8(IDS_ASH_VIRTUAL_DISPLAY_NAME);
        break;
      default:
        name = state->display_name();
    }

    if (name.empty())
      name = l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

    bool has_overscan = state->has_overscan();
    int64_t id = state->display_id();
    ids.insert(id);

    displays.push_back(DisplayInfo(id, name, has_overscan));
    DisplayInfo& new_info = displays.back();
    new_info.set_sys_path(state->sys_path());
    new_info.set_device_scale_factor(device_scale_factor);
    new_info.SetBounds(display_bounds);
    new_info.set_native(true);
    new_info.set_is_aspect_preserving_scaling(
        state->is_aspect_preserving_scaling());
    if (dpi)
      new_info.set_device_dpi(dpi);

    std::vector<DisplayMode> display_modes =
        (state->type() == ui::DISPLAY_CONNECTION_TYPE_INTERNAL)
            ? GetInternalDisplayModeList(new_info, *state)
            : GetExternalDisplayModeList(*state);
    new_info.SetDisplayModes(display_modes);

    new_info.set_available_color_profiles(
        Shell::GetInstance()
            ->display_configurator()
            ->GetAvailableColorCalibrationProfiles(id));
    new_info.set_maximum_cursor_size(state->maximum_cursor_size());
  }

  AssociateTouchscreens(
      &displays, ui::DeviceDataManager::GetInstance()->touchscreen_devices());
  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);

  // For the purposes of user activity detection, ignore synthetic mouse events
  // that are triggered by screen resizes: http://crbug.com/360634
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (user_activity_detector)
    user_activity_detector->OnDisplayPowerChanging();
}

void DisplayChangeObserver::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState failed_new_state) {
  // If display configuration failed during startup, simply update the display
  // manager with detected displays. If no display is detected, it will
  // create a pseudo display.
  if (Shell::GetInstance()->display_manager()->GetNumDisplays() == 0)
    OnDisplayModeChanged(displays);
}

void DisplayChangeObserver::OnAppTerminating() {
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  Shell::GetInstance()->display_configurator()->PrepareForExit();
}

// static
float DisplayChangeObserver::FindDeviceScaleFactor(float dpi) {
  for (size_t i = 0; i < arraysize(kThresholdTable); ++i) {
    if (dpi > kThresholdTable[i].dpi)
      return kThresholdTable[i].device_scale_factor;
  }
  return 1.0f;
}

void DisplayChangeObserver::OnTouchscreenDeviceConfigurationChanged() {
  OnDisplayModeChanged(
      Shell::GetInstance()->display_configurator()->cached_displays());
}

}  // namespace ash
