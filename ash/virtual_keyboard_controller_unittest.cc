// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include "ash/shell.h"
#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kDisableSmartVirtualKeyboard);
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

class VirtualKeyboardControllerAutoTest : public VirtualKeyboardControllerTest,
                                          public VirtualKeyboardObserver {
 public:
  VirtualKeyboardControllerAutoTest() : notified_(false), suppressed_(false) {}
  ~VirtualKeyboardControllerAutoTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    // Set the current list of devices to empty so that they don't interfere
    // with the test.
    UpdateKeyboardDevices(std::vector<ui::KeyboardDevice>());
    UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
    Shell::GetInstance()->system_tray_notifier()->AddVirtualKeyboardObserver(
        this);
  }

  void TearDown() override {
    Shell::GetInstance()->system_tray_notifier()->RemoveVirtualKeyboardObserver(
        this);
    AshTestBase::TearDown();
  }

  void OnKeyboardSuppressionChanged(bool suppressed) override {
    notified_ = true;
    suppressed_ = suppressed;
  }

  void ResetObserver() {
    suppressed_ = false;
    notified_ = false;
  }

  bool IsVirtualKeyboardSuppressed() { return suppressed_; }

  bool notified() { return notified_; }

 private:
  // Whether the observer method was called.
  bool notified_;

  // Whether the keeyboard is suppressed.
  bool suppressed_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerAutoTest);
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

TEST_F(VirtualKeyboardControllerAutoTest, SuppressedIfExternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1,
                            ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen",
                            gfx::Size(1024, 768)));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboards);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  Shell::GetInstance()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  Shell::GetInstance()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  UpdateKeyboardDevices(std::vector<ui::KeyboardDevice>());
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_FALSE(IsVirtualKeyboardSuppressed());
}

// Tests handling multiple keyboards. Catches crbug.com/430252
TEST_F(VirtualKeyboardControllerAutoTest, HandleMultipleKeyboardsPresent) {
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  keyboards.push_back(ui::KeyboardDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  keyboards.push_back(ui::KeyboardDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboards);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

}  // namespace test
}  // namespace ash
