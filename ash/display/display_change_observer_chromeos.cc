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
const unsigned int kHighDensityDPIThreshold = 160;

// 1 inch in mm.
const float kInchInMm = 25.4f;

// Resolution list are sorted by the area in pixels and the larger
// one comes first.
struct ResolutionSorter {
  bool operator()(const Resolution& a, const Resolution& b) {
    return a.size.width() * a.size.height() > b.size.width() * b.size.height();
  }
};

}  // namespace

// static
std::vector<Resolution> DisplayChangeObserver::GetResolutionList(
    const OutputConfigurator::OutputSnapshot& output) {
  typedef std::map<std::pair<int,int>, Resolution> ResolutionMap;
  ResolutionMap resolution_map;

  for (std::map<RRMode, OutputConfigurator::ModeInfo>::const_iterator it =
       output.mode_infos.begin(); it != output.mode_infos.end(); ++it) {
    const OutputConfigurator::ModeInfo& mode_info = it->second;
    const std::pair<int, int> size(mode_info.width, mode_info.height);
    const Resolution resolution(gfx::Size(mode_info.width, mode_info.height),
                                mode_info.interlaced);

    // Add the resolution if it isn't already present and override interlaced
    // resolutions with non-interlaced ones.
    ResolutionMap::iterator resolution_it = resolution_map.find(size);
    if (resolution_it == resolution_map.end())
      resolution_map.insert(std::make_pair(size, resolution));
    else if (resolution_it->second.interlaced && !resolution.interlaced)
      resolution_it->second = resolution;
  }

  std::vector<Resolution> resolution_list;
  for (ResolutionMap::const_iterator iter = resolution_map.begin();
       iter != resolution_map.end();
       ++iter) {
    resolution_list.push_back(iter->second);
  }
  std::sort(resolution_list.begin(), resolution_list.end(), ResolutionSorter());
  return resolution_list;
}

DisplayChangeObserver::DisplayChangeObserver() {
  Shell::GetInstance()->AddShellObserver(this);
}

DisplayChangeObserver::~DisplayChangeObserver() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

chromeos::OutputState DisplayChangeObserver::GetStateForDisplayIds(
    const std::vector<int64>& display_ids) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshForceMirrorMode)) {
    return chromeos::STATE_DUAL_MIRROR;
  }

  CHECK_EQ(2U, display_ids.size());
  DisplayIdPair pair = std::make_pair(display_ids[0], display_ids[1]);
  DisplayLayout layout = Shell::GetInstance()->display_manager()->
      layout_store()->GetRegisteredDisplayLayout(pair);
  return layout.mirrored ?
      chromeos::STATE_DUAL_MIRROR : chromeos::STATE_DUAL_EXTENDED;
}

bool DisplayChangeObserver::GetResolutionForDisplayId(int64 display_id,
                                                      int* width,
                                                      int* height) const {
  gfx::Size resolution;
  if (!Shell::GetInstance()->display_manager()->
      GetSelectedResolutionForDisplayId(display_id, &resolution)) {
    return false;
  }

  *width = resolution.width();
  *height = resolution.height();
  return true;
}

void DisplayChangeObserver::OnDisplayModeChanged(
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  std::vector<DisplayInfo> displays;
  std::set<int64> ids;
  for (size_t i = 0; i < outputs.size(); ++i) {
    const OutputConfigurator::OutputSnapshot& output = outputs[i];

    if (output.is_internal &&
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

    std::vector<Resolution> resolutions;
    if (!output.is_internal)
      resolutions = GetResolutionList(output);

    std::string name = output.is_internal ?
        l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME) :
        chromeos::GetDisplayName(output.output);
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
    displays.back().set_resolutions(resolutions);
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
