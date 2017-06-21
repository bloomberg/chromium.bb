// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/hid/fake_input_service_linux.h"
#include "device/hid/input_service_linux.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using device::InputServiceLinux;
using testing::_;

using InputDeviceInfo = InputServiceLinux::InputDeviceInfo;

namespace {

void SetUpBluetoothMock(
    scoped_refptr<
        testing::NiceMock<device::MockBluetoothAdapter> > mock_adapter,
    bool is_present) {
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(testing::Return(is_present));

  EXPECT_CALL(*mock_adapter, IsPowered())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_adapter, GetDevices()).WillRepeatedly(
      testing::Return(device::BluetoothAdapter::ConstDeviceList()));
}

}  // namespace

namespace chromeos {

class HidDetectionTest : public OobeBaseTest {
 public:
  typedef device::InputServiceLinux::InputDeviceInfo InputDeviceInfo;

  HidDetectionTest() : weak_ptr_factory_(this) {
    InputServiceProxy::SetUseUIThreadForTesting(true);
    HidDetectionTest::InitInputService();
  }

  ~HidDetectionTest() override {}

  void InitInputService() {
    InputServiceLinux::SetForTesting(
        base::MakeUnique<device::FakeInputServiceLinux>());
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
    SetUpBluetoothMock(mock_adapter_, true);
  }

  void AddUsbMouse(const std::string& mouse_id) {
    InputDeviceInfo mouse;
    mouse.id = mouse_id;
    mouse.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
    mouse.type = InputDeviceInfo::TYPE_USB;
    mouse.is_mouse = true;
    AddDeviceForTesting(mouse);
  }

  void AddUsbKeyboard(const std::string& keyboard_id) {
    InputDeviceInfo keyboard;
    keyboard.id = keyboard_id;
    keyboard.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
    keyboard.type = InputDeviceInfo::TYPE_USB;
    keyboard.is_keyboard = true;
    AddDeviceForTesting(keyboard);
  }

 private:
  void AddDeviceForTesting(const InputDeviceInfo& info) {
    static_cast<device::FakeInputServiceLinux*>(
        device::InputServiceLinux::GetInstance())
        ->AddDeviceForTesting(info);
  }

  scoped_refptr<
      testing::NiceMock<device::MockBluetoothAdapter> > mock_adapter_;

  base::WeakPtrFactory<HidDetectionTest> weak_ptr_factory_;

 DISALLOW_COPY_AND_ASSIGN(HidDetectionTest);
};

class HidDetectionSkipTest : public HidDetectionTest {
 public:
  HidDetectionSkipTest() {
    AddUsbMouse("mouse");
    AddUsbKeyboard("keyboard");
  }

  void SetUpOnMainThread() override {
    HidDetectionTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(HidDetectionTest, NoDevicesConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
}

IN_PROC_BROWSER_TEST_F(HidDetectionSkipTest, BothDevicesPreConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
}

}  // namespace chromeos
