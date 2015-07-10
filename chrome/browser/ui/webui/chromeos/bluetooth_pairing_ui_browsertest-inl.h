// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

class BluetoothPairingUITest : public WebUIBrowserTest {
 public:
  BluetoothPairingUITest();
  ~BluetoothPairingUITest() override;

  void ShowDialog();

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  scoped_ptr<device::MockBluetoothDevice> mock_device_;
};

BluetoothPairingUITest::BluetoothPairingUITest() {}
BluetoothPairingUITest::~BluetoothPairingUITest() {}

void BluetoothPairingUITest::ShowDialog() {
  mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillRepeatedly(testing::Return(true));

  const bool kNotPaired = false;
  const bool kNotConnected = false;
  mock_device_.reset(
      new testing::NiceMock<device::MockBluetoothDevice>(
          nullptr,
          0,
          "fake_bluetooth_device",
          "11:12:13:14:15:16",
          kNotPaired,
          kNotConnected));

  chromeos::BluetoothPairingDialog* dialog =
      new chromeos::BluetoothPairingDialog(
          browser()->window()->GetNativeWindow(), mock_device_.get());
  dialog->Show();

  content::WebUI* webui = dialog->GetWebUIForTest();
  content::WebContents* webui_webcontents = webui->GetWebContents();
  content::WaitForLoadStop(webui_webcontents);
  SetWebUIInstance(webui);
}
