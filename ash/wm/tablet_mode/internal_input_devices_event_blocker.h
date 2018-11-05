// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_
#define ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// Helper class to temporarily disable the internal touchpad and keyboard.
class ASH_EXPORT InternalInputDevicesEventBlocker
    : public ui::InputDeviceEventObserver {
 public:
  InternalInputDevicesEventBlocker();
  ~InternalInputDevicesEventBlocker() override;

  // ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;
  void OnTouchpadDeviceConfigurationChanged() override;

  void UpdateInternalInputDevices(bool should_block);

  bool is_blocked() const { return is_blocked_; }

 private:
  bool HasInternalTouchpad();
  bool HasInternalKeyboard();

  bool is_blocked_ = false;

  DISALLOW_COPY_AND_ASSIGN(InternalInputDevicesEventBlocker);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_INTERNAL_INPUT_DEVICES_EVENT_BLOCKER_H_
