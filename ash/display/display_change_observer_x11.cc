// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_x11.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include <X11/extensions/Xrandr.h>

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/message_pump_aurax11.h"
#include "ui/base/x/x11_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"

namespace ash {
namespace internal {

namespace {

// The DPI threshold to detect high density screen.
// Higher DPI than this will use device_scale_factor=2.
// Note: This value has to be kept in sync with the mouse/touchpad driver
// which controls mouse pointer acceleration. If you need to update this value,
// please update the bug (crosbug.com/31628) first and make sure that the
// driver will use the same value.
// This value also has to be kept in sync with the value in
// chromeos/display/output_configurator.cc. See crbug.com/130188
const unsigned int kHighDensityDPIThreshold = 160;

// 1 inch in mm.
const float kInchInMm = 25.4f;

XRRModeInfo* FindMode(XRRScreenResources* screen_resources, XID current_mode) {
  for (int m = 0; m < screen_resources->nmode; m++) {
    XRRModeInfo *mode = &screen_resources->modes[m];
    if (mode->id == current_mode)
      return mode;
  }
  return NULL;
}

bool CompareDisplayY(const gfx::Display& lhs, const gfx::Display& rhs) {
  return lhs.bounds_in_pixel().y() < rhs.bounds_in_pixel().y();
}

// A list of bogus sizes in mm that X detects and should be ignored.
// See crbug.com/136533.
const unsigned long kInvalidDisplaySizeList[][2] = {
  {160, 100},
  {160, 90},
  {50, 40},
  {40, 30},
};

// Returns true if the size nifo in the output_info isn't valid
// and should be ignored.
bool ShouldIgnoreSize(XRROutputInfo *output_info) {
  if (output_info->mm_width == 0 || output_info->mm_height == 0) {
    LOG(WARNING) << "No display size available";
    return true;
  }
  for (unsigned long i = 0 ; i < arraysize(kInvalidDisplaySizeList); ++i) {
    const unsigned long* size = kInvalidDisplaySizeList[i];
    if (output_info->mm_width == size[0] && output_info->mm_height == size[1]) {
      LOG(WARNING) << "Black listed display size detected:"
                   << size[0] << "x" << size[1];
      return true;
    }
  }
  return false;
}

}  // namespace

DisplayChangeObserverX11::DisplayChangeObserverX11()
    : xdisplay_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_event_base_(0) {
  int error_base_ignored;
  XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);
}

DisplayChangeObserverX11::~DisplayChangeObserverX11() {
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

  std::vector<gfx::Display> displays;
  std::set<int> y_coords;
  std::set<int64> ids;
  for (int output_index = 0; output_index < screen_resources->noutput;
       output_index++) {
    XRROutputInfo *output_info =
        XRRGetOutputInfo(xdisplay_,
                         screen_resources,
                         screen_resources->outputs[output_index]);
    if (output_info->connection != RR_Connected) {
      XRRFreeOutputInfo(output_info);
      continue;
    }
    XRRCrtcInfo* crtc_info = crtc_info_map[output_info->crtc];
    if (!crtc_info) {
      LOG(WARNING) << "Crtc not found for output: output_index="
                   << output_index;
      continue;
    }
    XRRModeInfo* mode = FindMode(screen_resources, crtc_info->mode);
    if (!mode) {
      LOG(WARNING) << "Could not find a mode for the output: output_index="
                   << output_index;
      continue;
    }
    // Mirrored monitors have the same y coordinates.
    if (y_coords.find(crtc_info->y) != y_coords.end())
      continue;
    displays.push_back(gfx::Display());

    float device_scale_factor = 1.0f;
    if (!ShouldIgnoreSize(output_info) &&
        (kInchInMm * mode->width / output_info->mm_width) >
        kHighDensityDPIThreshold) {
      device_scale_factor = 2.0f;
    }
    displays.back().SetScaleAndBounds(
        device_scale_factor,
        gfx::Rect(crtc_info->x, crtc_info->y, mode->width, mode->height));

    uint16 manufacturer_id = 0;
    uint16 product_code = 0;
    if (ui::GetOutputDeviceData(screen_resources->outputs[output_index],
                                &manufacturer_id, &product_code, NULL) &&
        manufacturer_id != 0) {
      // An ID based on display's index will be assigned later if this call
      // fails.
      int64 new_id = gfx::Display::GetID(
          manufacturer_id, product_code, output_index);
      if (ids.find(new_id) == ids.end()) {
        displays.back().set_id(new_id);
        ids.insert(new_id);
      }
    }

    y_coords.insert(crtc_info->y);
    XRRFreeOutputInfo(output_info);
  }

  // Free all allocated resources.
  for (std::map<XID, XRRCrtcInfo*>::const_iterator iter = crtc_info_map.begin();
       iter != crtc_info_map.end(); ++iter) {
    XRRFreeCrtcInfo(iter->second);
  }
  XRRFreeScreenResources(screen_resources);

  // PowerManager lays out the outputs vertically. Sort them by Y
  // coordinates.
  std::sort(displays.begin(), displays.end(), CompareDisplayY);
  int64 id = 0;
  for (std::vector<gfx::Display>::iterator iter = displays.begin();
       iter != displays.end(); ++iter) {
    if (iter->id() == gfx::Display::kInvalidDisplayID) {
      iter->set_id(id);
      ++id;
    }
  }
  // DisplayManager can be null during the boot.
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);
}

}  // namespace internal
}  // namespace ash
