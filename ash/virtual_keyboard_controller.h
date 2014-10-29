// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ui/events/input_device_event_observer.h"

namespace ash {

// This class observes input device changes for the virtual keyboard.
class ASH_EXPORT VirtualKeyboardController
    : public ShellObserver,
      public ui::InputDeviceEventObserver {
 public:
  VirtualKeyboardController();
  ~VirtualKeyboardController() override;

  // ShellObserver:
  virtual void OnMaximizeModeStarted() override;
  virtual void OnMaximizeModeEnded() override;

  // ui::InputDeviceObserver:
  // TODO(rsadam@): Remove when autovirtual keyboard flag is on by default.
  virtual void OnTouchscreenDeviceConfigurationChanged() override;
  virtual void OnKeyboardDeviceConfigurationChanged() override;

 private:
  // Updates the list of active input devices.
  void UpdateDevices();

  // Updates the keyboard state.
  void UpdateKeyboardEnabled();

  // Creates the keyboard if |enabled|, else destroys it.
  void SetKeyboardEnabled(bool enabled);

  // True if an external keyboard is connected.
  bool has_external_keyboard_;
  // True if an internal keyboard is connected.
  bool has_internal_keyboard_;
  // True if a touchscreen is connected.
  bool has_touchscreen_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardController);
};

}  // namespace ash

#endif  // ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
