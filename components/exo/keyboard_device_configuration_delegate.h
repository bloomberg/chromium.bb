// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_H_
#define COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_H_

namespace exo {
class Keyboard;

// Used as an extension to the KeyboardDelegate.
class KeyboardDeviceConfigurationDelegate {
 public:
  // Called at the top of the keyboard's destructor, to give observers a chance
  // to remove themselves.
  virtual void OnKeyboardDestroying(Keyboard* keyboard) = 0;

  // Called when used keyboard type changed.
  virtual void OnKeyboardTypeChanged(bool is_physical) = 0;

 protected:
  virtual ~KeyboardDeviceConfigurationDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_H_
