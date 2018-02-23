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
    int64_t display_id,
    display::Display::Rotation rotation,
    const gfx::Rect& native_bounds_in_pixel) {
  gfx::SizeF touchscreen_size = gfx::SizeF(touchscreen.size);

  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = display_id;
  touch_device_transform.device_id = touchscreen.id;
  touch_device_transform.transform.Translate(native_bounds_in_pixel.x(),
                                             native_bounds_in_pixel.y());

  float touchscreen_width = touchscreen_size.width();
  float touchscreen_height = touchscreen_size.height();

  // If the display orientation is rotated between portrait and landscape,
  // the width and height of the touchscreen must be swapped as well.
  if (rotation == display::Display::Rotation::ROTATE_90 ||
      rotation == display::Display::Rotation::ROTATE_270) {
    touchscreen_width = touchscreen_size.height();
    touchscreen_height = touchscreen_size.width();
  }

  touch_device_transform.transform.Scale(
      native_bounds_in_pixel.width() / touchscreen_width,
      native_bounds_in_pixel.height() / touchscreen_height);

  return touch_device_transform;
}

}  // namespace

CastTouchDeviceManager::CastTouchDeviceManager() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

CastTouchDeviceManager::~CastTouchDeviceManager() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void CastTouchDeviceManager::OnTouchscreenDeviceConfigurationChanged() {
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::OnDisplayConfigured(
    int64_t display_id,
    display::Display::Rotation rotation,
    const gfx::Rect& native_bounds_in_pixel) {
  display_id_ = display_id;
  display_rotation_ = rotation;
  native_display_bounds_in_pixel_ = native_bounds_in_pixel;
  UpdateTouchscreenConfiguration();
}

void CastTouchDeviceManager::UpdateTouchscreenConfiguration() {
  const std::vector<ui::TouchscreenDevice>& touchscreen_devices =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  if (native_display_bounds_in_pixel_ == gfx::Rect())
    return;  // Waiting for display configuration.
  std::vector<ui::TouchDeviceTransform> touch_device_transforms;

  // All touchscreens are mapped onto primary display.
  for (const auto& touchscreen : touchscreen_devices) {
    touch_device_transforms.push_back(
        GetDeviceTransform(touchscreen, display_id_, display_rotation_,
                           native_display_bounds_in_pixel_));
  }

  ui::DeviceDataManager::GetInstance()->ConfigureTouchDevices(
      touch_device_transforms);
}

}  // namespace shell
}  // namespace chromecast
