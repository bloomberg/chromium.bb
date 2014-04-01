// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/fake_input_device_settings.h"

namespace chromeos {
namespace system {

FakeInputDeviceSettings::FakeInputDeviceSettings() {}

FakeInputDeviceSettings::~FakeInputDeviceSettings() {}

// Overriden from InputDeviceSettings.
void FakeInputDeviceSettings::TouchpadExists(
    const DeviceExistsCallback& callback) {
  callback.Run(true);
}

void FakeInputDeviceSettings::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  current_touchpad_settings_.Update(settings, NULL);
}

void FakeInputDeviceSettings::SetTouchpadSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetSensitivity(value);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTapToClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapToClick(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetThreeFingerClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetThreeFingerClick(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetTapDragging(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapDragging(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::SetNaturalScroll(bool enabled) {
  TouchpadSettings settings;
  settings.SetNaturalScroll(enabled);
  UpdateTouchpadSettings(settings);
}

void FakeInputDeviceSettings::MouseExists(
    const DeviceExistsCallback& callback) {
  callback.Run(false);
}

void FakeInputDeviceSettings::UpdateMouseSettings(
    const MouseSettings& settings) {
  current_mouse_settings_.Update(settings, NULL);
}

void FakeInputDeviceSettings::SetMouseSensitivity(int value) {
  MouseSettings settings;
  settings.SetSensitivity(value);
  UpdateMouseSettings(settings);
}

void FakeInputDeviceSettings::SetPrimaryButtonRight(bool right) {
  MouseSettings settings;
  settings.SetPrimaryButtonRight(right);
  UpdateMouseSettings(settings);
}

bool FakeInputDeviceSettings::ForceKeyboardDrivenUINavigation() {
  return false;
}

void FakeInputDeviceSettings::ReapplyTouchpadSettings() {
}

void FakeInputDeviceSettings::ReapplyMouseSettings() {
}

}  // namespace system
}  // namespace chromeos
