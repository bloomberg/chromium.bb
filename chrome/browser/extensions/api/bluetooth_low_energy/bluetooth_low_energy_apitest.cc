// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothUUID;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattService;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothGattService;
using testing::Return;
using testing::_;

namespace utils = extension_function_test_utils;

namespace {

const char kTestLeDeviceAddress[] = "11:22:33:44:55:66";
const char kTestLeDeviceName[] = "Test LE Device";

const char kTestServiceId0[] = "service_id0";
const char kTestServiceUuid0[] = "1234";
const char kTestServiceId1[] = "service_id1";
const char kTestServiceUuid1[] = "5678";

class BluetoothLowEnergyApiTest : public ExtensionApiTest {
 public:
  BluetoothLowEnergyApiTest() {}

  virtual ~BluetoothLowEnergyApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    empty_extension_ = utils::CreateEmptyExtension();
    SetUpMocks();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_CALL(*mock_adapter_, RemoveObserver(_));
  }

  void SetUpMocks() {
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    EXPECT_CALL(*mock_adapter_, GetDevices())
        .WillOnce(Return(BluetoothAdapter::ConstDeviceList()));

    event_router()->SetAdapterForTesting(mock_adapter_);

    device_.reset(
        new testing::NiceMock<MockBluetoothDevice>(mock_adapter_,
                                                   0,
                                                   kTestLeDeviceName,
                                                   kTestLeDeviceAddress,
                                                   false /* paired */,
                                                   true /* connected */));

    service0_.reset(new testing::NiceMock<MockBluetoothGattService>(
        device_.get(),
        kTestServiceId0,
        BluetoothUUID(kTestServiceUuid0),
        true /* is_primary */,
        false /* is_local */));

    service1_.reset(new testing::NiceMock<MockBluetoothGattService>(
        device_.get(),
        kTestServiceId1,
        BluetoothUUID(kTestServiceUuid1),
        false /* is_primary */,
        false /* is_local */));
  }

 protected:
  extensions::BluetoothLowEnergyEventRouter* event_router() {
    return extensions::BluetoothLowEnergyAPI::Get(browser()->profile())
        ->event_router();
  }

  testing::StrictMock<MockBluetoothAdapter>* mock_adapter_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattService> > service0_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattService> > service1_;

 private:
  scoped_refptr<extensions::Extension> empty_extension_;
};

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetServices) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattService*> services;
  services.push_back(service0_.get());
  services.push_back(service1_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattServices())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothGattService*>()))
      .WillOnce(Return(services));

  // Load and wait for setup.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_services")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetService) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(2)
      .WillOnce(Return(static_cast<BluetoothGattService*>(NULL)))
      .WillOnce(Return(service0_.get()));

  // Load and wait for setup.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_service")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ServiceEvents) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/service_events")));

  // Cause events to be sent to the extension.
  event_router()->DeviceAdded(mock_adapter_, device_.get());

  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattServiceAdded(device_.get(), service1_.get());
  event_router()->GattServiceChanged(service1_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device_.get(), service1_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedService) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load the extension and let it set up.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_removed_service")));

  // 1. getService success.
  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());

  ExtensionTestMessageListener get_service_success_listener("getServiceSuccess",
                                                            true);
  EXPECT_TRUE(get_service_success_listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device_.get());

  // 2. getService fail.
  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0)).Times(0);

  event_router()->GattServiceRemoved(device_.get(), service0_.get());

  ExtensionTestMessageListener get_service_fail_listener("getServiceFail",
                                                         true);
  EXPECT_TRUE(get_service_fail_listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device_.get());

  get_service_fail_listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

}  // namespace
