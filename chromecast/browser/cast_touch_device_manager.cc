// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_touch_device_manager.h"

#include "chromecast/graphics/cast_screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace chromecast {
namespace shell {
namespace {

ui::TouchDeviceTransform GetDeviceTransform(
    const ui::TouchscreenDevice& touchscreen,
    display::Display display) {
  gfx::RectF display_bounds = gfx::RectF(display.bounds());
  gfx::SizeF touchscreen_size = gfx::SizeF(touchscreen.size);

  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = display.id();
  touch_device_transform.device_id = touchscreen.id;
  touch_device_transform.transform.Translate(display_bounds.x(),
                                             display_bounds.y());
  touch_device_transform.transform.Scale(
      display_bounds.width() / touchscreen_size.width(),
      display_bounds.height() / touchscreen_size.height());

  return touch_device_transform;
}

}  // namespace

CastTouchDeviceManager::CastTouchDeviceManager(CastScreen* cast_screen)
    : cast_screen_(cast_screen) {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  cast_screen_->AddObserver(this);
  UpdateTouchscreenConfiguration();
}

CastTouchDeviceManager::~CastTouchDeviceManager() {
  cast_screen_->RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void CastTouchDeviceManager::OnTouchscreenDeviceConfigurationChanged() {
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::OnDisplayAdded(
    const display::Display& new_display) {
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::OnDisplayRemoved(
    const display::Display& old_display) {
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::UpdateTouchscreenConfiguration() {
  display::Display primary_display = cast_screen_->GetPrimaryDisplay();
  const std::vector<ui::TouchscreenDevice>& touchscreen_devices =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  std::vector<ui::TouchDeviceTransform> touch_device_transforms;

  // All touchscreens are mapped onto primary display.
  for (const auto& touchscreen : touchscreen_devices) {
    touch_device_transforms.push_back(
        GetDeviceTransform(touchscreen, primary_display));
  }

  ui::DeviceDataManager::GetInstance()->ConfigureTouchDevices(
      touch_device_transforms);
}

}  // namespace shell
}  // namespace chromecast
