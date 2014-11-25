// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

namespace chromeos {
namespace system {

namespace {

InputDeviceSettings* g_instance = nullptr;
InputDeviceSettings* g_test_instance = nullptr;

// InputDeviceSettings for Linux without X11 (a.k.a. Ozone).
class InputDeviceSettingsImplOzone : public InputDeviceSettings {
 public:
  InputDeviceSettingsImplOzone();

 protected:
  ~InputDeviceSettingsImplOzone() override {}

 private:
  // Overridden from InputDeviceSettings.
  void TouchpadExists(const DeviceExistsCallback& callback) override;
  void UpdateTouchpadSettings(const TouchpadSettings& settings) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void MouseExists(const DeviceExistsCallback& callback) override;
  void UpdateMouseSettings(const MouseSettings& settings) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;

  // Respective device setting objects.
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceSettingsImplOzone);
};

InputDeviceSettingsImplOzone::InputDeviceSettingsImplOzone() {
}

void InputDeviceSettingsImplOzone::TouchpadExists(
    const DeviceExistsCallback& callback) {
  NOTIMPLEMENTED();
}

void InputDeviceSettingsImplOzone::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  if (current_touchpad_settings_.Update(settings))
    ReapplyTouchpadSettings();
}

void InputDeviceSettingsImplOzone::SetTouchpadSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_touchpad_settings_.SetSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetNaturalScroll(bool enabled) {
  current_touchpad_settings_.SetNaturalScroll(enabled);
}

void InputDeviceSettingsImplOzone::SetTapToClick(bool enabled) {
  current_touchpad_settings_.SetTapToClick(enabled);
}

void InputDeviceSettingsImplOzone::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  current_touchpad_settings_.SetThreeFingerClick(enabled);
}

void InputDeviceSettingsImplOzone::SetTapDragging(bool enabled) {
  current_touchpad_settings_.SetTapDragging(enabled);
}

void InputDeviceSettingsImplOzone::MouseExists(
    const DeviceExistsCallback& callback) {
  NOTIMPLEMENTED();
}

void InputDeviceSettingsImplOzone::UpdateMouseSettings(
    const MouseSettings& update) {
  if (current_mouse_settings_.Update(update))
    ReapplyMouseSettings();
}

void InputDeviceSettingsImplOzone::SetMouseSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_mouse_settings_.SetSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetPrimaryButtonRight(bool right) {
  current_mouse_settings_.SetPrimaryButtonRight(right);
}

void InputDeviceSettingsImplOzone::ReapplyTouchpadSettings() {
  TouchpadSettings::Apply(current_touchpad_settings_, this);
}

void InputDeviceSettingsImplOzone::ReapplyMouseSettings() {
  MouseSettings::Apply(current_mouse_settings_, this);
}

}  // namespace

// static
InputDeviceSettings* InputDeviceSettings::Get() {
  if (g_test_instance)
    return g_test_instance;
  if (!g_instance)
    g_instance = new InputDeviceSettingsImplOzone;
  return g_instance;
}

// static
void InputDeviceSettings::SetSettingsForTesting(
    InputDeviceSettings* test_settings) {
  if (g_test_instance == test_settings)
    return;
  delete g_test_instance;
  g_test_instance = test_settings;
}

}  // namespace system
}  // namespace chromeos
