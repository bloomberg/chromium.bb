// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touchscreen_util.h"

#include <set>

#include "base/logging.h"
#include "ui/events/devices/input_device.h"

namespace ash {

void AssociateTouchscreens(std::vector<DisplayInfo>* displays,
                           const std::vector<ui::TouchscreenDevice>& devices) {
  DisplayInfo* internal_state = NULL;
  for (size_t i = 0; i < displays->size(); ++i) {
    DisplayInfo* state = &(*displays)[i];
    if (gfx::Display::IsInternalDisplayId(state->id()) &&
        !state->GetNativeModeSize().IsEmpty() &&
        state->touch_support() == gfx::Display::TOUCH_SUPPORT_UNKNOWN) {
      DCHECK(!internal_state);
      internal_state = state;
    }
    state->ClearInputDevices();
  }

  std::set<int> no_match_touchscreen;
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      // Don't try and map internal touchscreens to external displays.
      if (!internal_state)
        continue;
      VLOG(2) << "Found internal device for display " << internal_state->id()
              << " with device id " << devices[i].id << " size "
              << devices[i].size.ToString();
      internal_state->AddInputDevice(devices[i].id);
      internal_state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);
      continue;
    }

    bool found_mapping = false;
    for (size_t j = 0; j < displays->size(); ++j) {
      DisplayInfo* state = &(*displays)[j];
      if (state == internal_state)
        continue;

      const gfx::Size native_size = state->GetNativeModeSize();
      if (state->touch_support() == gfx::Display::TOUCH_SUPPORT_AVAILABLE ||
          native_size.IsEmpty())
        continue;

      // Allow 1 pixel difference between screen and touchscreen
      // resolutions. Because in some cases for monitor resolution
      // 1024x768 touchscreen's resolution would be 1024x768, but for
      // some 1023x767. It really depends on touchscreen's firmware
      // configuration.
      if (std::abs(native_size.width() - devices[i].size.width()) <= 1 &&
          std::abs(native_size.height() - devices[i].size.height()) <= 1) {
        state->AddInputDevice(devices[i].id);
        state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);

        VLOG(2) << "Found touchscreen for display " << state->id()
                << " with device id " << devices[i].id << " size "
                << devices[i].size.ToString();
        found_mapping = true;
        break;
      }
    }

    if (!found_mapping) {
      no_match_touchscreen.insert(devices[i].id);
      VLOG(2) << "No matching display for device id " << devices[i].id
              << " size " << devices[i].size.ToString();
    }
  }

  // Sometimes we can't find a matching screen for the touchscreen, e.g.
  // due to the touchscreen's reporting range having no correlation with the
  // screen's resolution. In this case, we arbitrarily assign unmatched
  // touchscreens to unmatched screens.
  for (std::set<int>::iterator it = no_match_touchscreen.begin();
       it != no_match_touchscreen.end();
       ++it) {
    for (size_t i = 0; i < displays->size(); ++i) {
      DisplayInfo* state = &(*displays)[i];
      if (!gfx::Display::IsInternalDisplayId(state->id()) &&
          !state->GetNativeModeSize().IsEmpty() &&
          state->touch_support() == gfx::Display::TOUCH_SUPPORT_UNKNOWN) {
        state->AddInputDevice(*it);
        state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);
        VLOG(2) << "Arbitrarily matching touchscreen " << *it << " to display "
                << state->id();
        break;
      }
    }
  }
}

}  // namespace ash
