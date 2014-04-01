// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"

namespace chromeos {
namespace system {

// This fake just memorizes current values of input devices settings.
class FakeInputDeviceSettings : public InputDeviceSettings {
 public:
  FakeInputDeviceSettings();
  virtual ~FakeInputDeviceSettings();

  // Overridden from InputDeviceSettings.
  virtual void TouchpadExists(const DeviceExistsCallback& callback) OVERRIDE;
  virtual void UpdateTouchpadSettings(const TouchpadSettings& settings)
      OVERRIDE;
  virtual void SetTouchpadSensitivity(int value) OVERRIDE;
  virtual void SetTapToClick(bool enabled) OVERRIDE;
  virtual void SetThreeFingerClick(bool enabled) OVERRIDE;
  virtual void SetTapDragging(bool enabled) OVERRIDE;
  virtual void MouseExists(const DeviceExistsCallback& callback) OVERRIDE;
  virtual void UpdateMouseSettings(const MouseSettings& settings) OVERRIDE;
  virtual void SetMouseSensitivity(int value) OVERRIDE;
  virtual void SetPrimaryButtonRight(bool right) OVERRIDE;
  virtual void SetNaturalScroll(bool enabled) OVERRIDE;

  virtual bool ForceKeyboardDrivenUINavigation() OVERRIDE;
  virtual void ReapplyTouchpadSettings() OVERRIDE;
  virtual void ReapplyMouseSettings() OVERRIDE;

  const TouchpadSettings& current_touchpad_settings() const {
    return current_touchpad_settings_;
  }

  const MouseSettings& current_mouse_settings() const {
    return current_mouse_settings_;
  }

 private:
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(FakeInputDeviceSettings);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_FAKE_INPUT_DEVICE_SETTINGS_H_
