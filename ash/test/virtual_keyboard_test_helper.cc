// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/virtual_keyboard_test_helper.h"

#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {
namespace test {

void VirtualKeyboardTestHelper::SuppressKeyboard() {
  ui::DeviceHotplugEventObserver* manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, gfx::Size(1024, 768), 0));
  manager->OnTouchscreenDevicesUpdated(screens);

  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.push_back(
      ui::KeyboardDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL));
  manager->OnKeyboardDevicesUpdated(keyboards);
}

}  // namespace test
}  // namespace ash
