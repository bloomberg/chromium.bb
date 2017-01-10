// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "chromeos/system/devicemode.h"
#include "content/public/browser/browser_thread.h"
#include "services/service_manager/runner/common/client_util.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromeos {
namespace system {

namespace {

InputDeviceSettings* g_instance = nullptr;

std::unique_ptr<ui::InputController> CreateStubInputControllerIfNecessary() {
  return service_manager::ServiceManagerIsRemote()
             ? ui::CreateStubInputController()
             : nullptr;
}

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
  InputDeviceSettings::FakeInterface* GetFakeInterface() override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  void SetTouchscreensEnabled(bool enabled) override;

  // TODO(sad): A stub input controller is used when running inside mus.
  // http://crbug.com/601981
  std::unique_ptr<ui::InputController> stub_controller_;

  // Cached InputController pointer. It should be fixed throughout the browser
  // session.
  ui::InputController* input_controller_;

  // Respective device setting objects.
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceSettingsImplOzone);
};

InputDeviceSettingsImplOzone::InputDeviceSettingsImplOzone()
    : stub_controller_(CreateStubInputControllerIfNecessary()),
      input_controller_(
          stub_controller_
              ? stub_controller_.get()
              : ui::OzonePlatform::GetInstance()->GetInputController()) {
  // Make sure the input controller does exist.
  DCHECK(input_controller_);
}

void InputDeviceSettingsImplOzone::TouchpadExists(
    const DeviceExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(input_controller_->HasTouchpad());
}

void InputDeviceSettingsImplOzone::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  if (current_touchpad_settings_.Update(settings))
    ReapplyTouchpadSettings();
}

void InputDeviceSettingsImplOzone::SetTouchpadSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_touchpad_settings_.SetSensitivity(value);
  input_controller_->SetTouchpadSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetNaturalScroll(bool enabled) {
  current_touchpad_settings_.SetNaturalScroll(enabled);
  input_controller_->SetNaturalScroll(enabled);
}

void InputDeviceSettingsImplOzone::SetTapToClick(bool enabled) {
  current_touchpad_settings_.SetTapToClick(enabled);
  input_controller_->SetTapToClick(enabled);
}

void InputDeviceSettingsImplOzone::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  current_touchpad_settings_.SetThreeFingerClick(enabled);
  input_controller_->SetThreeFingerClick(enabled);
}

void InputDeviceSettingsImplOzone::SetTapDragging(bool enabled) {
  current_touchpad_settings_.SetTapDragging(enabled);
  input_controller_->SetTapDragging(enabled);
}

void InputDeviceSettingsImplOzone::MouseExists(
    const DeviceExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(input_controller_->HasMouse());
}

void InputDeviceSettingsImplOzone::UpdateMouseSettings(
    const MouseSettings& update) {
  if (current_mouse_settings_.Update(update))
    ReapplyMouseSettings();
}

void InputDeviceSettingsImplOzone::SetMouseSensitivity(int value) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  current_mouse_settings_.SetSensitivity(value);
  input_controller_->SetMouseSensitivity(value);
}

void InputDeviceSettingsImplOzone::SetPrimaryButtonRight(bool right) {
  current_mouse_settings_.SetPrimaryButtonRight(right);
  input_controller_->SetPrimaryButtonRight(right);
}

void InputDeviceSettingsImplOzone::ReapplyTouchpadSettings() {
  TouchpadSettings::Apply(current_touchpad_settings_, this);
}

void InputDeviceSettingsImplOzone::ReapplyMouseSettings() {
  MouseSettings::Apply(current_mouse_settings_, this);
}

InputDeviceSettings::FakeInterface*
InputDeviceSettingsImplOzone::GetFakeInterface() {
  return nullptr;
}

void InputDeviceSettingsImplOzone::SetInternalTouchpadEnabled(bool enabled) {
  input_controller_->SetInternalTouchpadEnabled(enabled);
}

void InputDeviceSettingsImplOzone::SetTouchscreensEnabled(bool enabled) {
  input_controller_->SetTouchscreensEnabled(enabled);
}

}  // namespace

// static
InputDeviceSettings* InputDeviceSettings::Get() {
  if (!g_instance) {
    if (IsRunningAsSystemCompositor())
      g_instance = new InputDeviceSettingsImplOzone;
    else
      g_instance = new FakeInputDeviceSettings();
  }
  return g_instance;
}

}  // namespace system
}  // namespace chromeos
