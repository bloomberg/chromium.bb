// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/focus_client.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"

namespace exo {
namespace {

using KeyboardTest = test::ExoTestBase;

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate() {}

  // Overridden from KeyboardDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
  MOCK_CONST_METHOD1(CanAcceptKeyboardEventsForSurface, bool(Surface*));
  MOCK_METHOD2(OnKeyboardEnter,
               void(Surface*, const std::vector<ui::DomCode>&));
  MOCK_METHOD1(OnKeyboardLeave, void(Surface*));
  MOCK_METHOD3(OnKeyboardKey, void(base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD1(OnKeyboardModifiers, void(int));
};

class MockKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate {
 public:
  MockKeyboardDeviceConfigurationDelegate() {}

  // Overridden from KeyboardDeviceConfigurationDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
  MOCK_METHOD1(OnKeyboardTypeChanged, void(bool));
};

class MockKeyboardObserver : public KeyboardObserver {
 public:
  MockKeyboardObserver() {}

  // Overridden from KeyboardObserver:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
};

TEST_F(KeyboardTest, OnKeyboardEnter) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(surface->window());

  // Keyboard should try to set initial focus to surface.
  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(false));
  std::unique_ptr<Keyboard> keyboard(new Keyboard(&delegate));

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_A, 0);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  ui::DomCode expected_pressed_keys[] = {ui::DomCode::US_A};
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              std::vector<ui::DomCode>(
                                  expected_pressed_keys,
                                  expected_pressed_keys +
                                      arraysize(expected_pressed_keys))));
  focus_client->FocusWindow(nullptr);
  focus_client->FocusWindow(surface->window());
  // Surface should maintain keyboard focus when moved to top-level window.
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());

  keyboard.reset();
}

TEST_F(KeyboardTest, OnKeyboardLeave) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  MockKeyboardDelegate delegate;
  std::unique_ptr<Keyboard> keyboard(new Keyboard(&delegate));

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(), std::vector<ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);

  keyboard.reset();
}

TEST_F(KeyboardTest, OnKeyboardKey) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  MockKeyboardDelegate delegate;
  std::unique_ptr<Keyboard> keyboard(new Keyboard(&delegate));

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(), std::vector<ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate one press event for KEY_A.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  generator.PressKey(ui::VKEY_A, 0);
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_B, 0);

  // This should only generate one release event for KEY_A.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  generator.ReleaseKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);

  keyboard.reset();
}

TEST_F(KeyboardTest, OnKeyboardModifiers) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  MockKeyboardDelegate delegate;
  std::unique_ptr<Keyboard> keyboard(new Keyboard(&delegate));

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(), std::vector<ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should generate a modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_SHIFT_DOWN));
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);

  // This should generate another modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
  EXPECT_CALL(delegate,
              OnKeyboardModifiers(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  generator.PressKey(ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

  // This should generate a third modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  generator.ReleaseKey(ui::VKEY_B, 0);

  keyboard.reset();
}

TEST_F(KeyboardTest, OnKeyboardTypeChanged) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  ASSERT_TRUE(device_data_manager != nullptr);
  const std::vector<ui::InputDevice> keyboards =
      device_data_manager->GetKeyboardDevices();

  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->EnableTabletModeWindowManager(true);

  MockKeyboardDelegate delegate;
  std::unique_ptr<Keyboard> keyboard(new Keyboard(&delegate));
  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard->SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard->HasDeviceConfigurationDelegate());

  // Removing all keyboard devices in tablet mode calls
  // OnKeyboardTypeChanged() with false.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(false));
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnKeyboardDevicesUpdated(std::vector<ui::InputDevice>({}));

  // Re-adding keyboards calls OnKeyboardTypeChanged() with true;
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnKeyboardDevicesUpdated(keyboards);

  keyboard.reset();

  tablet_mode_controller->EnableTabletModeWindowManager(false);
}

TEST_F(KeyboardTest, KeyboardObserver) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  MockKeyboardDelegate delegate;
  auto keyboard = base::MakeUnique<Keyboard>(&delegate);
  MockKeyboardObserver observer;
  keyboard->AddObserver(&observer);

  EXPECT_CALL(observer, OnKeyboardDestroying(keyboard.get()));
  keyboard.reset();
}

TEST_F(KeyboardTest, NeedKeyboardKeyAcks) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  MockKeyboardDelegate delegate;
  auto keyboard = base::MakeUnique<Keyboard>(&delegate);

  EXPECT_FALSE(keyboard->AreKeyboardKeyAcksNeeded());
  keyboard->SetNeedKeyboardKeyAcks(true);
  EXPECT_TRUE(keyboard->AreKeyboardKeyAcksNeeded());
  keyboard->SetNeedKeyboardKeyAcks(false);
  EXPECT_FALSE(keyboard->AreKeyboardKeyAcksNeeded());

  keyboard.reset();
}

}  // namespace
}  // namespace exo
