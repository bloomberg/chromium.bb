// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "components/pairing/bluetooth_host_pairing_controller.h"
#include "components/pairing/shark_connection_listener.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/fake_input_service_linux.h"

namespace chromeos {

namespace {

class TestDelegate
    : public pairing_chromeos::BluetoothHostPairingController::TestDelegate {
 public:
  TestDelegate() {}
  ~TestDelegate() override {}

  // pairing_chromeos::BluetoothHostPairingController::Delegate override:
  void OnAdapterReset() override {
    finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitUntilAdapterReset() {
    if (finished_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 private:
  bool finished_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

// This is the class to simulate the OOBE process for devices that don't have
// sufficient input, i.e., the first screen of OOBE is the HID detection screen.
// The device will put itself in Bluetooth discoverable mode.
class BluetoothHostPairingNoInputTest : public OobeBaseTest {
 public:
  using InputDeviceInfo = device::InputServiceLinux::InputDeviceInfo;

  BluetoothHostPairingNoInputTest() {
    InputServiceProxy::SetThreadIdForTesting(content::BrowserThread::UI);
    device::InputServiceLinux::SetForTesting(
        base::MakeUnique<device::FakeInputServiceLinux>());
  }
  ~BluetoothHostPairingNoInputTest() override {}

  // OobeBaseTest override:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    delegate_.reset(new TestDelegate);
    if (controller()) {
      controller()->SetDelegateForTesting(delegate_.get());
      bluetooth_adapter_ = controller()->GetAdapterForTesting();
    }
  }

  pairing_chromeos::BluetoothHostPairingController* controller() {
    pairing_chromeos::SharkConnectionListener* shark_listener =
        WizardController::default_controller()
            ->GetSharkConnectionListenerForTesting();
    return shark_listener ? shark_listener->GetControllerForTesting() : nullptr;
  }

  device::BluetoothAdapter* bluetooth_adapter() {
    return bluetooth_adapter_.get();
  }

  TestDelegate* delegate() { return delegate_.get(); }

  void AddUsbMouse() {
    InputDeviceInfo mouse;
    mouse.id = "usb_mouse";
    mouse.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
    mouse.type = InputDeviceInfo::TYPE_USB;
    mouse.is_mouse = true;
    AddDeviceForTesting(mouse);
  }

  void AddUsbKeyboard() {
    InputDeviceInfo keyboard;
    keyboard.id = "usb_keyboard";
    keyboard.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
    keyboard.type = InputDeviceInfo::TYPE_USB;
    keyboard.is_keyboard = true;
    AddDeviceForTesting(keyboard);
  }

  void AddBluetoothMouse() {
    InputDeviceInfo mouse;
    mouse.id = "bluetooth_mouse";
    mouse.subsystem = InputDeviceInfo::SUBSYSTEM_INPUT;
    mouse.type = InputDeviceInfo::TYPE_BLUETOOTH;
    mouse.is_mouse = true;
    AddDeviceForTesting(mouse);
  }

 private:
  void AddDeviceForTesting(const InputDeviceInfo& info) {
    static_cast<device::FakeInputServiceLinux*>(
        device::InputServiceLinux::GetInstance())
        ->AddDeviceForTesting(info);
  }

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<TestDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothHostPairingNoInputTest);
};

// Test that in normal user OOBE login flow for devices lacking input devices,
// if there is no Bluetooth device connected, the Bluetooth adapter should be
// disabled when OOBE reaches login screen (which means OOBE has been completed)
IN_PROC_BROWSER_TEST_F(BluetoothHostPairingNoInputTest,
                       NoBluetoothDeviceConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
  EXPECT_EQ(bluetooth_adapter()->IsPowered(), true);
  WizardController::default_controller()->SkipToLoginForTesting(
      LoginScreenContext());
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  delegate()->WaitUntilAdapterReset();
  EXPECT_EQ(bluetooth_adapter()->IsPowered(), false);
}

// Test that in normal user OOBE login flow for devices lacking input devices,
// if there is any Bluetooth device connected, the Bluetooth adapter should not
// be disabled after OOBE completes.
IN_PROC_BROWSER_TEST_F(BluetoothHostPairingNoInputTest,
                       BluetoothDeviceConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
  AddBluetoothMouse();
  EXPECT_EQ(bluetooth_adapter()->IsPowered(), true);
  WizardController::default_controller()->SkipToLoginForTesting(
      LoginScreenContext());
  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  delegate()->WaitUntilAdapterReset();
  EXPECT_EQ(bluetooth_adapter()->IsPowered(), true);
}

// This is the class to simulate the OOBE process for devices that have
// sufficient input, i.e., the first screen of OOBE is the network screen.
// The device will not put itself in Bluetooth discoverable mode until the user
// manually trigger it using the proper accelerator.
class BluetoothHostPairingWithInputTest
    : public BluetoothHostPairingNoInputTest {
 public:
  BluetoothHostPairingWithInputTest() {
    AddUsbMouse();
    AddUsbKeyboard();
  }
  ~BluetoothHostPairingWithInputTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothHostPairingWithInputTest);
};

// Test that in normal user OOBE login flow for devices that have input devices,
// the Bluetooth is disabled by default.
IN_PROC_BROWSER_TEST_F(BluetoothHostPairingWithInputTest,
                       BluetoothDisableByDefault) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_FALSE(controller());
  EXPECT_FALSE(bluetooth_adapter());
}

}  // namespace chromeos
