// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_
#define CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_

#include "base/macros.h"
#include "chromecast/graphics/cast_screen.h"
#include "ui/display/display_observer.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace chromecast {
namespace shell {

// Manages touchscreen->display mapping for cast browser.
class CastTouchDeviceManager : public display::DisplayObserver,
                               public ui::InputDeviceEventObserver {
 public:
  explicit CastTouchDeviceManager(CastScreen* screen);
  ~CastTouchDeviceManager() override;

  // ui::InputDeviceEventObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void UpdateTouchscreenConfiguration();

  CastScreen* cast_screen_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CastTouchDeviceManager);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_
