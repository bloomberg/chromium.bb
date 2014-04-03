// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"

namespace chromeos {
namespace system {

namespace internal {

// Objects of this class are intended to store values of type T, but might have
// "unset" state. Object will be in "unset" state until Set is called first
// time.
template <typename T>
class Optional {
 public:
  Optional()
    : value_(),
      is_set_(false) {
  }

  Optional& operator=(const Optional& other) {
    if (&other != this) {
      value_ = other.value_;
      is_set_ = other.is_set_;
    }
    return *this;
  }

  void Set(const T& value) {
    is_set_ = true;
    value_ = value;
  }

  bool is_set() const {
    return is_set_;
  }

  T value() const {
    DCHECK(is_set());
    return value_;
  }

  // Tries to update |this| with |update|. If |update| is unset or has same
  // value as |this| method returns false. Otherwise |this| takes value of
  // |update| and returns true.
  bool Update(const Optional& update) {
    if (update.is_set_ && (!is_set_ || value_ != update.value_)) {
      value_ = update.value_;
      is_set_ = true;
      return true;
    }
    return false;
  }

 private:
  T value_;
  bool is_set_;
};

}  // namespace internal

// Min/max possible pointer sensitivity values. Defined in CrOS inputcontrol
// scripts (see kTpControl/kMouseControl in the source file).
const int kMinPointerSensitivity = 1;
const int kMaxPointerSensitivity = 5;

// Auxiliary class used to update several touchpad settings at a time. User
// should set any number of settings and pass object to UpdateTouchpadSettings
// method of InputDeviceSettings.
// Objects of this class have no default values for settings, so it is error
// to call Get* method before calling corresponding Set* method at least
// once.
class TouchpadSettings {
 public:
  TouchpadSettings();
  TouchpadSettings& operator=(const TouchpadSettings& other);

  void SetSensitivity(int value);
  int GetSensitivity() const;

  void SetTapToClick(bool enabled);
  bool GetTapToClick() const;

  void SetThreeFingerClick(bool enabled);
  bool GetThreeFingerClick() const;

  void SetTapDragging(bool enabled);
  bool GetTapDragging() const;

  void SetNaturalScroll(bool enabled);
  bool GetNaturalScroll() const;

  // Updates |this| with |settings|. If at least one setting was updated returns
  // true.
  // |argv| is filled with arguments of script, that should be launched in order
  // to apply update. This argument is optional and could be NULL.
  bool Update(const TouchpadSettings& settings, std::vector<std::string>* argv);

 private:
  internal::Optional<int> sensitivity_;
  internal::Optional<bool> tap_to_click_;
  internal::Optional<bool> three_finger_click_;
  internal::Optional<bool> tap_dragging_;
  internal::Optional<bool> natural_scroll_;
};

// Auxiliary class used to update several mouse settings at a time. User
// should set any number of settings and pass object to UpdateMouseSettings
// method of InputDeviceSettings.
// Objects of this class have no default values for settings, so it is error
// to call Get* method before calling corresponding Set* method at least
// once.
class MouseSettings {
 public:
  MouseSettings();
  MouseSettings& operator=(const MouseSettings& other);

  void SetSensitivity(int value);
  int GetSensitivity() const;

  void SetPrimaryButtonRight(bool right);
  bool GetPrimaryButtonRight() const;

  // Updates |this| with |settings|. If at least one setting was updated returns
  // true.
  // |argv| is filled with arguments of script, that should be launched in order
  // to apply update. This argument is optional and could be NULL.
  bool Update(const MouseSettings& update, std::vector<std::string>* argv);

 private:
  internal::Optional<int> sensitivity_;
  internal::Optional<bool> primary_button_right_;
};

class InputDeviceSettings {
 public:
  typedef base::Callback<void(bool)> DeviceExistsCallback;

  virtual ~InputDeviceSettings() {}

  // Returns current instance of InputDeviceSettings.
  static InputDeviceSettings* Get();

  // Replaces current instance with |test_settings|. Takes ownership of
  // |test_settings|. Default implementation could be returned back by passing
  // NULL to this method.
  static void SetSettingsForTesting(InputDeviceSettings* test_settings);

  // Calls |callback| asynchronously after determining if a touchpad is
  // connected.
  virtual void TouchpadExists(const DeviceExistsCallback& callback) = 0;

  // Updates several touchpad settings at a time. Updates only settings that
  // are set in |settings| object. It is more efficient to use this method to
  // update several settings then calling Set* methods one by one.
  virtual void UpdateTouchpadSettings(const TouchpadSettings& settings) = 0;

  // Sets the touchpad sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetTouchpadSensitivity(int value) = 0;

  // Turns tap to click on/off.
  virtual void SetTapToClick(bool enabled) = 0;

  // Switch for three-finger click.
  virtual void SetThreeFingerClick(bool enabled) = 0;

  // Turns tap-dragging on/off.
  virtual void SetTapDragging(bool enabled) = 0;

  // Turns natural scrolling on/off for all devices except wheel mice
  virtual void SetNaturalScroll(bool enabled) = 0;

  // Calls |callback| asynchronously after determining if a mouse is connected.
  virtual void MouseExists(const DeviceExistsCallback& callback) = 0;

  // Updates several mouse settings at a time. Updates only settings that
  // are set in |settings| object. It is more efficient to use this method to
  // update several settings then calling Set* methods one by one.
  virtual void UpdateMouseSettings(const MouseSettings& settings) = 0;

  // Sets the mouse sensitivity in the range [kMinPointerSensitivity,
  // kMaxPointerSensitivity].
  virtual void SetMouseSensitivity(int value) = 0;

  // Sets the primary mouse button to the right button if |right| is true.
  virtual void SetPrimaryButtonRight(bool right) = 0;

  // Returns true if UI should implement enhanced keyboard support for cases
  // where other input devices like mouse are absent.
  virtual bool ForceKeyboardDrivenUINavigation() = 0;

  // Reapplies previously set touchpad settings.
  virtual void ReapplyTouchpadSettings() = 0;

  // Reapplies previously set mouse settings.
  virtual void ReapplyMouseSettings() = 0;
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
