// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touchscreen_util.h"

#include <set>

#include "base/logging.h"

namespace ash {

void AssociateTouchscreens(std::vector<DisplayInfo>* displays,
                           const std::vector<ui::TouchscreenDevice>& devices) {
  std::set<int> no_match_touchscreen;
  int internal_touchscreen = -1;
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_internal) {
      internal_touchscreen = i;
      break;
    }
  }

  DisplayInfo* internal_state = NULL;
  for (size_t i = 0; i < displays->size(); ++i) {
    DisplayInfo* state = &(*displays)[i];
    if (state->id() == gfx::Display::InternalDisplayId() &&
        !state->GetNativeModeSize().IsEmpty() &&
        state->touch_support() == gfx::Display::TOUCH_SUPPORT_UNKNOWN) {
      internal_state = state;
      break;
    }
  }

  if (internal_state && internal_touchscreen >= 0) {
    internal_state->set_touch_device_id(devices[internal_touchscreen].id);
    internal_state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);
    VLOG(2) << "Found internal touchscreen for internal display "
            << internal_state->id() << " touch_device_id "
            << internal_state->touch_device_id() << " size "
            << devices[internal_touchscreen].size.ToString();
  }

  for (size_t i = 0; i < devices.size(); ++i) {
    if (internal_state && internal_state->touch_device_id() == devices[i].id)
      continue;

    bool found_mapping = false;
    for (size_t j = 0; j < displays->size(); ++j) {
      DisplayInfo* state = &(*displays)[j];
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
        state->set_touch_device_id(devices[i].id);
        state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);

        VLOG(2) << "Found touchscreen for display " << state->id()
                << " touch_device_id " << state->touch_device_id() << " size "
                << devices[i].size.ToString();
        found_mapping = true;
        break;
      }
    }

    if (!found_mapping) {
      no_match_touchscreen.insert(devices[i].id);
      VLOG(2) << "No matching display for touch_device_id " << devices[i].id
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
      if (state->id() != gfx::Display::InternalDisplayId() &&
          !state->GetNativeModeSize().IsEmpty() &&
          state->touch_support() == gfx::Display::TOUCH_SUPPORT_UNKNOWN) {
        state->set_touch_device_id(*it);
        state->set_touch_support(gfx::Display::TOUCH_SUPPORT_AVAILABLE);
        VLOG(2) << "Arbitrarily matching touchscreen "
                << state->touch_device_id() << " to display " << state->id();
        break;
      }
    }
  }
}

}  // namespace ash
