// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/keyboard/keyboard_layout_delegate.h"

namespace ash {

// This class observes input device changes for the virtual keyboard.
class ASH_EXPORT VirtualKeyboardController
    : public TabletModeObserver,
      public ui::InputDeviceEventObserver,
      public keyboard::KeyboardLayoutDelegate {
 public:
  VirtualKeyboardController();
  ~VirtualKeyboardController() override;

  // TabletModeObserver:
  // TODO(rsadam@): Remove when autovirtual keyboard flag is on by default.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ui::InputDeviceObserver:
  void OnTouchscreenDeviceConfigurationChanged() override;
  void OnKeyboardDeviceConfigurationChanged() override;

  // Toggles whether the presence of an external keyboard should be ignored
  // when determining whether or not to show the on-screen keyboard.
  void ToggleIgnoreExternalKeyboard();

  // keyboard::KeyboardLayoutDelegate
  void MoveKeyboardToDisplay(int64_t display_id) override;
  void MoveKeyboardToTouchableDisplay() override;

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
  // True if the presence of an external keyboard should be ignored.
  bool ignore_external_keyboard_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardController);
};

}  // namespace ash

#endif  // ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
