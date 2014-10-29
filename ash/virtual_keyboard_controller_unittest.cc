// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "ui/events/device_data_manager.h"
#include "ui/events/device_hotplug_event_observer.h"
#include "ui/events/input_device.h"
#include "ui/events/keyboard_device.h"
#include "ui/events/touchscreen_device.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace test {

class VirtualKeyboardControllerTest : public AshTestBase {
 public:
  VirtualKeyboardControllerTest() {}
  virtual ~VirtualKeyboardControllerTest() {}

  void UpdateTouchscreenDevices(
      std::vector<ui::TouchscreenDevice> touchscreen_devices) {
    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnTouchscreenDevicesUpdated(touchscreen_devices);
  }

  void UpdateKeyboardDevices(std::vector<ui::KeyboardDevice> keyboard_devices) {
    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnKeyboardDevicesUpdated(keyboard_devices);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateKeyboardDevices(std::vector<ui::KeyboardDevice>());
    UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerTest);
};

TEST_F(VirtualKeyboardControllerTest, EnabledDuringMaximizeMode) {
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // Toggle maximized mode on.
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Toggle maximized mode off.
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

class VirtualKeyboardControllerAutoTest : public VirtualKeyboardControllerTest {
 public:
  void SetUp() override {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kAutoVirtualKeyboard);
    VirtualKeyboardControllerTest::SetUp();
  }
};

// Tests that the onscreen keyboard is disabled if an internal keyboard is
// present.
TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfInternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1,
                            ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen",
                            gfx::Size(1024, 768)));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboards);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // Remove the internal keyboard. Virtual keyboard should now show.
  UpdateKeyboardDevices(std::vector<ui::KeyboardDevice>());
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Replug in the internal keyboard. Virtual keyboard should hide.
  UpdateKeyboardDevices(keyboards);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfNoTouchScreen) {
  std::vector<ui::TouchscreenDevice> devices;
  // Add a touchscreen. Keyboard should deploy.
  devices.push_back(
      ui::TouchscreenDevice(1,
                            ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                            "Touchscreen",
                            gfx::Size(800, 600)));
  UpdateTouchscreenDevices(devices);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Remove touchscreen. Keyboard should hide.
  UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

}  // namespace test
}  // namespace ash
