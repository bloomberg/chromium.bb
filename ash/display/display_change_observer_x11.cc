// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_x11.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include <X11/extensions/Xrandr.h>

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util_x11.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_aurax11.h"
#include "chromeos/display/output_util.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"

namespace ash {
namespace internal {
namespace {

// The DPI threshold to detect high density screen.
// Higher DPI than this will use device_scale_factor=2.
const unsigned int kHighDensityDPIThreshold = 160;

// 1 inch in mm.
const float kInchInMm = 25.4f;

int64 GetDisplayId(XID output, size_t output_index) {
  int64 display_id;
  if (chromeos::GetDisplayId(output, output_index, &display_id))
    return display_id;
  return gfx::Display::kInvalidDisplayID;
}

}  // namespace

DisplayChangeObserverX11::DisplayChangeObserverX11()
    : xdisplay_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_event_base_(0) {
  int error_base_ignored;
  XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

  Shell::GetInstance()->AddShellObserver(this);
}

DisplayChangeObserverX11::~DisplayChangeObserverX11() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

chromeos::OutputState DisplayChangeObserverX11::GetStateForDisplayIds(
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

bool DisplayChangeObserverX11::GetResolutionForDisplayId(int64 display_id,
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

void DisplayChangeObserverX11::OnDisplayModeChanged() {
  XRRScreenResources* screen_resources =
      XRRGetScreenResources(xdisplay_, x_root_window_);
  std::map<XID, XRRCrtcInfo*> crtc_info_map;

  for (int c = 0; c < screen_resources->ncrtc; c++) {
    XID crtc_id = screen_resources->crtcs[c];
    XRRCrtcInfo *crtc_info =
        XRRGetCrtcInfo(xdisplay_, screen_resources, crtc_id);
    crtc_info_map[crtc_id] = crtc_info;
  }

  std::vector<DisplayInfo> displays;
  std::set<int64> ids;
  for (int output_index = 0; output_index < screen_resources->noutput;
       output_index++) {
    XID output = screen_resources->outputs[output_index];
    XRROutputInfo *output_info =
        XRRGetOutputInfo(xdisplay_, screen_resources, output);

    const bool is_internal = chromeos::IsInternalOutputName(
        std::string(output_info->name));

    if (is_internal &&
        gfx::Display::InternalDisplayId() == gfx::Display::kInvalidDisplayID) {
      int64 id = GetDisplayId(output, output_index);
      // Fallback to output index. crbug.com/180100
      gfx::Display::SetInternalDisplayId(
          id == gfx::Display::kInvalidDisplayID ? output_index : id);
    }

    if (output_info->connection != RR_Connected) {
      XRRFreeOutputInfo(output_info);
      continue;
    }
    const XRRCrtcInfo* crtc_info = crtc_info_map[output_info->crtc];
    if (!crtc_info) {
      LOG(WARNING) << "Crtc not found for output: output_index="
                   << output_index;
      continue;
    }
    const XRRModeInfo* mode =
        chromeos::FindModeInfo(screen_resources, crtc_info->mode);
    if (!mode) {
      LOG(WARNING) << "Could not find a mode for the output: output_index="
                   << output_index;
      continue;
    }

    float device_scale_factor = 1.0f;
    if (!ShouldIgnoreSize(output_info->mm_width, output_info->mm_height) &&
        (kInchInMm * mode->width / output_info->mm_width) >
        kHighDensityDPIThreshold) {
      device_scale_factor = 2.0f;
    }
    gfx::Rect display_bounds(
        crtc_info->x, crtc_info->y, mode->width, mode->height);

    std::vector<Resolution> resolutions;
    if (!is_internal)
      resolutions = GetResolutionList(screen_resources, output_info);

    XRRFreeOutputInfo(output_info);

    std::string name = is_internal ?
        l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME) :
        chromeos::GetDisplayName(output);
    if (name.empty())
      name = l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

    bool has_overscan = false;
    chromeos::GetOutputOverscanFlag(output, &has_overscan);

    int64 id = GetDisplayId(output, output_index);

    // If ID is invalid or there is an duplicate, just use output index.
    if (id == gfx::Display::kInvalidDisplayID || ids.find(id) != ids.end())
      id = output_index;
    ids.insert(id);

    displays.push_back(DisplayInfo(id, name, has_overscan));
    displays.back().set_device_scale_factor(device_scale_factor);
    displays.back().SetBounds(display_bounds);
    displays.back().set_native(true);
    displays.back().set_resolutions(resolutions);
  }

  // Free all allocated resources.
  for (std::map<XID, XRRCrtcInfo*>::const_iterator iter = crtc_info_map.begin();
       iter != crtc_info_map.end(); ++iter) {
    XRRFreeCrtcInfo(iter->second);
  }
  XRRFreeScreenResources(screen_resources);

  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);
}

void DisplayChangeObserverX11::OnAppTerminating() {
#if defined(USE_ASH)
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  Shell::GetInstance()->output_configurator()->Stop();
#endif
}

}  // namespace internal
}  // namespace ash
