// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/maximize_mode/touchpad_and_keyboard_disabler.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"

namespace ash {

ScopedDisableInternalMouseAndKeyboardOzone::
    ScopedDisableInternalMouseAndKeyboardOzone()
    : disabler_(nullptr) {
  // InputDeviceControllerClient may be null in tests.
  if (Shell::Get()->shell_delegate()->GetInputDeviceControllerClient())
    disabler_ = new TouchpadAndKeyboardDisabler;
}

ScopedDisableInternalMouseAndKeyboardOzone::
    ~ScopedDisableInternalMouseAndKeyboardOzone() {
  if (disabler_)
    disabler_->Destroy();
}

}  // namespace ash
