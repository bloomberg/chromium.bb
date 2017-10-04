// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/chromeos/device/input_service_test_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "device/hid/public/interfaces/input_service.mojom.h"

namespace chromeos {

class HIDDetectionScreenTest : public WizardInProcessBrowserTest {
 public:
  HIDDetectionScreenTest()
      : WizardInProcessBrowserTest(OobeScreen::SCREEN_OOBE_HID_DETECTION) {}

 protected:
  void SetUpOnMainThread() override {
    helper_.reset(new InputServiceTestHelper);
    WizardInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WizardController::default_controller());

    hid_detection_screen_ = static_cast<HIDDetectionScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_OOBE_HID_DETECTION));
    ASSERT_TRUE(hid_detection_screen_);
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              hid_detection_screen_);
    ASSERT_TRUE(hid_detection_screen_->view_);

    hid_detection_screen()->SetAdapterInitialPoweredForTesting(false);
    helper_->SetProxy(&hid_detection_screen_->input_service_proxy_);
  }

  void TearDownOnMainThread() override {
    helper_->ClearProxy();
    WizardInProcessBrowserTest::TearDownOnMainThread();
  }

  InputServiceTestHelper* helper() { return helper_.get(); }

  HIDDetectionScreen* hid_detection_screen() { return hid_detection_screen_; }

  scoped_refptr<device::BluetoothAdapter> adapter() {
    return hid_detection_screen_->GetAdapterForTesting();
  }

  const ::login::ScreenContext* context() const {
    return &hid_detection_screen_->context_;
  }

 private:
  HIDDetectionScreen* hid_detection_screen_;
  std::unique_ptr<InputServiceTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreenTest);
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, MouseKeyboardStates) {
  // NOTE: State strings match those in hid_detection_screen.cc.
  // No devices added yet
  EXPECT_EQ("searching",
            context()->GetString(HIDDetectionScreen::kContextKeyMouseState));
  EXPECT_EQ("searching",
            context()->GetString(HIDDetectionScreen::kContextKeyKeyboardState));
  EXPECT_FALSE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  // Generic connection types. Unlike the pointing device, which may be a tablet
  // or touchscreen, the keyboard only reports usb and bluetooth states.
  helper()->AddDeviceToService(true,
                               device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_TRUE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  helper()->AddDeviceToService(false,
                               device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected",
            context()->GetString(HIDDetectionScreen::kContextKeyMouseState));
  EXPECT_EQ("usb",
            context()->GetString(HIDDetectionScreen::kContextKeyKeyboardState));
  EXPECT_TRUE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  // Remove generic devices, add usb devices.
  helper()->RemoveDeviceFromService(true);
  helper()->RemoveDeviceFromService(false);
  EXPECT_FALSE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  helper()->AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  helper()->AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);
  EXPECT_EQ("usb",
            context()->GetString(HIDDetectionScreen::kContextKeyMouseState));
  EXPECT_EQ("usb",
            context()->GetString(HIDDetectionScreen::kContextKeyKeyboardState));
  EXPECT_TRUE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  // Remove usb devices, add bluetooth devices.
  helper()->RemoveDeviceFromService(true);
  helper()->RemoveDeviceFromService(false);
  EXPECT_FALSE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));

  helper()->AddDeviceToService(true,
                               device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  helper()->AddDeviceToService(false,
                               device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  EXPECT_EQ("paired",
            context()->GetString(HIDDetectionScreen::kContextKeyMouseState));
  EXPECT_EQ("paired",
            context()->GetString(HIDDetectionScreen::kContextKeyKeyboardState));
  EXPECT_TRUE(context()->GetBoolean(
      HIDDetectionScreen::kContextKeyContinueButtonEnabled));
}

// Test that if there is any Bluetooth device connected on HID screen, the
// Bluetooth adapter should not be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, BluetoothDeviceConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
  EXPECT_TRUE(adapter()->IsPowered());

  // Add a pair of USB mouse/keyboard so that |pointing_device_connect_type_|
  // and |keyboard_device_connect_type_| are
  // device::mojom::InputDeviceType::TYPE_USB.
  helper()->AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  helper()->AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);

  // Add another pair of Bluetooth mouse/keyboard.
  helper()->AddDeviceToService(true,
                               device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  helper()->AddDeviceToService(false,
                               device::mojom::InputDeviceType::TYPE_BLUETOOTH);

  // Simulate the user's click on "Continue" button.
  hid_detection_screen()->OnContinueButtonClicked();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  // The adapter should not be powered off at this moment.
  EXPECT_TRUE(adapter()->IsPowered());
}

// Test that if there is no Bluetooth device connected on HID screen, the
// Bluetooth adapter should be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, NoBluetoothDeviceConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
  EXPECT_TRUE(adapter()->IsPowered());

  helper()->AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  helper()->AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);

  // Simulate the user's click on "Continue" button.
  hid_detection_screen()->OnContinueButtonClicked();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  // The adapter should be powered off at this moment.
  EXPECT_FALSE(adapter()->IsPowered());
}

}  // namespace chromeos
