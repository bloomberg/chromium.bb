// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_change_handler.h"

#include "chrome/browser/chromeos/system/input_device_settings.h"

namespace chromeos {
namespace system {

DeviceChangeHandler::DeviceChangeHandler()
    : pointer_device_observer_(new PointerDeviceObserver) {
  pointer_device_observer_->AddObserver(this);
  pointer_device_observer_->Init();

  // Apply settings on startup.
  TouchpadExists(true);
  MouseExists(true);
}

DeviceChangeHandler::~DeviceChangeHandler() {
  pointer_device_observer_->RemoveObserver(this);
}

// When we detect a touchpad is attached, apply touchpad settings that was
// cached inside InputDeviceSettings.
void DeviceChangeHandler::TouchpadExists(bool exists) {
  if (!exists)
    return;
  system::InputDeviceSettings::Get()->ReapplyTouchpadSettings();
}

// When we detect a mouse is attached, apply mouse settings that was cached
// inside InputDeviceSettings.
void DeviceChangeHandler::MouseExists(bool exists) {
  if (!exists)
    return;
  system::InputDeviceSettings::Get()->ReapplyMouseSettings();
}

}  // namespace system
}  // namespace chromeos
