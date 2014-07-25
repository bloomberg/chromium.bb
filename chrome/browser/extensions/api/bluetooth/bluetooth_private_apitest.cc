// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/bluetooth_private.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::WithArgs;
using testing::WithoutArgs;

namespace bt = extensions::api::bluetooth;
namespace bt_private = extensions::api::bluetooth_private;

namespace {

const char kTestExtensionId[] = "jofgjdphhceggjecimellaapdjjadibj";
const char kAdapterName[] = "Helix";
const char kDeviceName[] = "Red";
}

class BluetoothPrivateApiTest : public ExtensionApiTest {
 public:
  BluetoothPrivateApiTest()
      : adapter_name_(kAdapterName),
        adapter_powered_(false),
        adapter_discoverable_(false) {}

  virtual ~BluetoothPrivateApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestExtensionId);
    mock_adapter_ = new NiceMock<MockBluetoothAdapter>();
    event_router()->SetAdapterForTest(mock_adapter_.get());
    mock_device_.reset(new NiceMock<MockBluetoothDevice>(mock_adapter_.get(),
                                                         0,
                                                         kDeviceName,
                                                         "11:12:13:14:15:16",
                                                         false,
                                                         false));
    ON_CALL(*mock_adapter_, GetDevice(mock_device_->GetAddress()))
        .WillByDefault(Return(mock_device_.get()));
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(Return(true));
  }

  virtual void TearDownOnMainThread() OVERRIDE {}

  extensions::BluetoothEventRouter* event_router() {
    return extensions::BluetoothAPI::Get(browser()->profile())
        ->event_router();
  }

  void SetName(const std::string& name, const base::Closure& callback) {
    adapter_name_ = name;
    callback.Run();
  }

  void SetPowered(bool powered, const base::Closure& callback) {
    adapter_powered_ = powered;
    callback.Run();
  }

  void SetDiscoverable(bool discoverable, const base::Closure& callback) {
    adapter_discoverable_ = discoverable;
    callback.Run();
  }

  void DispatchPairingEvent(bt_private::PairingEventType pairing_event_type) {
    bt_private::PairingEvent pairing_event;
    pairing_event.pairing = pairing_event_type;
    pairing_event.device.name.reset(new std::string(kDeviceName));
    pairing_event.device.address = mock_device_->GetAddress();
    pairing_event.device.vendor_id_source = bt::VENDOR_ID_SOURCE_USB;
    pairing_event.device.type = bt::DEVICE_TYPE_PHONE;

    scoped_ptr<base::ListValue> args =
        bt_private::OnPairing::Create(pairing_event);
    scoped_ptr<extensions::Event> event(
        new extensions::Event(bt_private::OnPairing::kEventName, args.Pass()));
    extensions::EventRouter::Get(browser()->profile())
        ->DispatchEventToExtension(kTestExtensionId, event.Pass());
  }

  void DispatchAuthorizePairingEvent() {
    DispatchPairingEvent(bt_private::PAIRING_EVENT_TYPE_REQUESTAUTHORIZATION);
  }

  void DispatchPincodePairingEvent() {
    DispatchPairingEvent(bt_private::PAIRING_EVENT_TYPE_REQUESTPINCODE);
  }

  void DispatchPasskeyPairingEvent() {
    DispatchPairingEvent(bt_private::PAIRING_EVENT_TYPE_REQUESTPASSKEY);
  }

 protected:
  std::string adapter_name_;
  bool adapter_powered_;
  bool adapter_discoverable_;

  scoped_refptr<NiceMock<MockBluetoothAdapter> > mock_adapter_;
  scoped_ptr<NiceMock<MockBluetoothDevice> > mock_device_;
};

IN_PROC_BROWSER_TEST_F(BluetoothPrivateApiTest, SetAdapterState) {
  ON_CALL(*mock_adapter_, GetName())
      .WillByDefault(ReturnPointee(&adapter_name_));
  ON_CALL(*mock_adapter_, IsPowered())
      .WillByDefault(ReturnPointee(&adapter_powered_));
  ON_CALL(*mock_adapter_, IsDiscoverable())
      .WillByDefault(ReturnPointee(&adapter_discoverable_));

  EXPECT_CALL(*mock_adapter_, SetName("Dome", _, _)).WillOnce(
      WithArgs<0, 1>(Invoke(this, &BluetoothPrivateApiTest::SetName)));
  EXPECT_CALL(*mock_adapter_, SetPowered(true, _, _)).WillOnce(
      WithArgs<0, 1>(Invoke(this, &BluetoothPrivateApiTest::SetPowered)));
  EXPECT_CALL(*mock_adapter_, SetDiscoverable(true, _, _)).WillOnce(
      WithArgs<0, 1>(Invoke(this, &BluetoothPrivateApiTest::SetDiscoverable)));

  ASSERT_TRUE(RunComponentExtensionTest("bluetooth_private/adapter_state"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothPrivateApiTest, NoBluetoothAdapter) {
  ON_CALL(*mock_adapter_, IsPresent())
      .WillByDefault(Return(false));
  ASSERT_TRUE(RunComponentExtensionTest("bluetooth_private/no_adapter"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothPrivateApiTest, CancelPairing) {
  InSequence s;
  EXPECT_CALL(*mock_adapter_,
              AddPairingDelegate(
                  _, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH))
      .WillOnce(WithoutArgs(Invoke(
          this, &BluetoothPrivateApiTest::DispatchAuthorizePairingEvent)));
  EXPECT_CALL(*mock_device_, ExpectingConfirmation())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_device_, CancelPairing());
  ASSERT_TRUE(RunComponentExtensionTest("bluetooth_private/cancel_pairing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothPrivateApiTest, PincodePairing) {
  EXPECT_CALL(*mock_adapter_,
              AddPairingDelegate(
                  _, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH))
      .WillOnce(WithoutArgs(
          Invoke(this, &BluetoothPrivateApiTest::DispatchPincodePairingEvent)));
  EXPECT_CALL(*mock_device_, ExpectingPinCode()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_device_, SetPinCode("abbbbbbk"));
  ASSERT_TRUE(RunComponentExtensionTest("bluetooth_private/pincode_pairing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothPrivateApiTest, PasskeyPairing) {
  EXPECT_CALL(*mock_adapter_,
              AddPairingDelegate(
                  _, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH))
      .WillOnce(WithoutArgs(
          Invoke(this, &BluetoothPrivateApiTest::DispatchPasskeyPairingEvent)));
  EXPECT_CALL(*mock_device_, ExpectingPasskey()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_device_, SetPasskey(900531));
  ASSERT_TRUE(RunComponentExtensionTest("bluetooth_private/passkey_pairing"))
      << message_;
}
