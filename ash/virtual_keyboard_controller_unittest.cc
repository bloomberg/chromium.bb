// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

class VirtualKeyboardControllerTest : public AshTestBase {
 public:
  VirtualKeyboardControllerTest() {}
  ~VirtualKeyboardControllerTest() override {}

  void UpdateTouchscreenDevices(
      std::vector<ui::TouchscreenDevice> touchscreen_devices) {
    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnTouchscreenDevicesUpdated(touchscreen_devices);
  }

  void UpdateKeyboardDevices(std::vector<ui::InputDevice> keyboard_devices) {
    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnKeyboardDevicesUpdated(keyboard_devices);
  }

  // Sets the event blocker on the maximized window controller.
  void SetEventBlocker(
      std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> blocker) {
    Shell::Get()->tablet_mode_controller()->event_blocker_ = std::move(blocker);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateKeyboardDevices(std::vector<ui::InputDevice>());
    UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerTest);
};

// Mock event blocker that enables the internal keyboard when it's destructor
// is called.
class MockEventBlocker : public ScopedDisableInternalMouseAndKeyboard {
 public:
  MockEventBlocker() {}
  ~MockEventBlocker() override {
    std::vector<ui::InputDevice> keyboard_devices;
    keyboard_devices.push_back(ui::InputDevice(
        1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
    ui::DeviceHotplugEventObserver* manager =
        ui::DeviceDataManager::GetInstance();
    manager->OnKeyboardDevicesUpdated(keyboard_devices);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventBlocker);
};

// Tests that reenabling keyboard devices while shutting down does not
// cause the Virtual Keyboard Controller to crash. See crbug.com/446204.
TEST_F(VirtualKeyboardControllerTest, RestoreKeyboardDevices) {
  // Toggle tablet mode on.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> blocker(
      new MockEventBlocker);
  SetEventBlocker(std::move(blocker));
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
    UpdateKeyboardDevices(std::vector<ui::InputDevice>());
    UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
    Shell::Get()->system_tray_notifier()->AddVirtualKeyboardObserver(this);
  }

  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveVirtualKeyboardObserver(this);
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
// present and tablet mode is disabled.
TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfInternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // Remove the internal keyboard. Virtual keyboard should now show.
  UpdateKeyboardDevices(std::vector<ui::InputDevice>());
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Replug in the internal keyboard. Virtual keyboard should hide.
  UpdateKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfNoTouchScreen) {
  std::vector<ui::TouchscreenDevice> devices;
  // Add a touchscreen. Keyboard should deploy.
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL,
                            "Touchscreen", gfx::Size(800, 600), 0));
  UpdateTouchscreenDevices(devices);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Remove touchscreen. Keyboard should hide.
  UpdateTouchscreenDevices(std::vector<ui::TouchscreenDevice>());
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

TEST_F(VirtualKeyboardControllerAutoTest, SuppressedIfExternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  UpdateKeyboardDevices(std::vector<ui::InputDevice>());
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_FALSE(IsVirtualKeyboardSuppressed());
}

// Tests handling multiple keyboards. Catches crbug.com/430252
TEST_F(VirtualKeyboardControllerAutoTest, HandleMultipleKeyboardsPresent) {
  std::vector<ui::InputDevice> keyboards;
  keyboards.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  keyboards.push_back(ui::InputDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  keyboards.push_back(ui::InputDevice(
      3, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboards);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

// Tests tablet mode interaction without disabling the internal keyboard.
TEST_F(VirtualKeyboardControllerAutoTest, EnabledDuringTabletMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  UpdateKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  // Toggle tablet mode on.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  // Toggle tablet mode off.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

// Tests that keyboard gets suppressed in tablet mode.
TEST_F(VirtualKeyboardControllerAutoTest, SuppressedInMaximizedMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  keyboard_devices.push_back(ui::InputDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "Keyboard"));
  UpdateKeyboardDevices(keyboard_devices);
  // Toggle tablet mode on.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  keyboard_devices.pop_back();
  UpdateKeyboardDevices(keyboard_devices);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
  ASSERT_TRUE(notified());
  ASSERT_FALSE(IsVirtualKeyboardSuppressed());
  // Toggle tablet mode oFF.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  ASSERT_FALSE(keyboard::IsKeyboardEnabled());
}

class VirtualKeyboardControllerAlwaysEnabledTest
    : public VirtualKeyboardControllerAutoTest {
 public:
  VirtualKeyboardControllerAlwaysEnabledTest()
      : VirtualKeyboardControllerAutoTest() {}
  ~VirtualKeyboardControllerAlwaysEnabledTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    VirtualKeyboardControllerAutoTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerAlwaysEnabledTest);
};

// Tests that the controller cannot suppress the keyboard if the virtual
// keyboard always enabled flag is active.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest, DoesNotSuppressKeyboard) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  UpdateTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  UpdateKeyboardDevices(keyboard_devices);
  ASSERT_TRUE(keyboard::IsKeyboardEnabled());
}

}  // namespace ash
