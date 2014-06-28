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
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothUUID;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;
using device::BluetoothGattNotifySession;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothGattCharacteristic;
using device::MockBluetoothGattConnection;
using device::MockBluetoothGattDescriptor;
using device::MockBluetoothGattService;
using device::MockBluetoothGattNotifySession;
using extensions::BluetoothLowEnergyEventRouter;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::SaveArg;
using testing::_;

namespace utils = extension_function_test_utils;

namespace {

// Test service constants.
const char kTestLeDeviceAddress0[] = "11:22:33:44:55:66";
const char kTestLeDeviceName0[] = "Test LE Device 0";

const char kTestLeDeviceAddress1[] = "77:88:99:AA:BB:CC";
const char kTestLeDeviceName1[] = "Test LE Device 1";

const char kTestServiceId0[] = "service_id0";
const char kTestServiceUuid0[] = "1234";

const char kTestServiceId1[] = "service_id1";
const char kTestServiceUuid1[] = "5678";

// Test characteristic constants.
const char kTestCharacteristicId0[] = "char_id0";
const char kTestCharacteristicUuid0[] = "1211";
const BluetoothGattCharacteristic::Properties kTestCharacteristicProperties0 =
    BluetoothGattCharacteristic::kPropertyBroadcast |
    BluetoothGattCharacteristic::kPropertyRead |
    BluetoothGattCharacteristic::kPropertyWriteWithoutResponse |
    BluetoothGattCharacteristic::kPropertyIndicate;
const uint8 kTestCharacteristicDefaultValue0[] = {0x01, 0x02, 0x03, 0x04, 0x05};

const char kTestCharacteristicId1[] = "char_id1";
const char kTestCharacteristicUuid1[] = "1212";
const BluetoothGattCharacteristic::Properties kTestCharacteristicProperties1 =
    BluetoothGattCharacteristic::kPropertyRead |
    BluetoothGattCharacteristic::kPropertyWrite |
    BluetoothGattCharacteristic::kPropertyNotify;
const uint8 kTestCharacteristicDefaultValue1[] = {0x06, 0x07, 0x08};

const char kTestCharacteristicId2[] = "char_id2";
const char kTestCharacteristicUuid2[] = "1213";
const BluetoothGattCharacteristic::Properties kTestCharacteristicProperties2 =
    BluetoothGattCharacteristic::kPropertyNone;

// Test descriptor constants.
const char kTestDescriptorId0[] = "desc_id0";
const char kTestDescriptorUuid0[] = "1221";
const uint8 kTestDescriptorDefaultValue0[] = {0x01, 0x02, 0x03};

const char kTestDescriptorId1[] = "desc_id1";
const char kTestDescriptorUuid1[] = "1222";
const uint8 kTestDescriptorDefaultValue1[] = {0x04, 0x05};

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

    device0_.reset(
        new testing::NiceMock<MockBluetoothDevice>(mock_adapter_,
                                                   0,
                                                   kTestLeDeviceName0,
                                                   kTestLeDeviceAddress0,
                                                   false /* paired */,
                                                   true /* connected */));

    device1_.reset(
        new testing::NiceMock<MockBluetoothDevice>(mock_adapter_,
                                                   0,
                                                   kTestLeDeviceName1,
                                                   kTestLeDeviceAddress1,
                                                   false /* paired */,
                                                   false /* connected */));

    service0_.reset(new testing::NiceMock<MockBluetoothGattService>(
        device0_.get(),
        kTestServiceId0,
        BluetoothUUID(kTestServiceUuid0),
        true /* is_primary */,
        false /* is_local */));

    service1_.reset(new testing::NiceMock<MockBluetoothGattService>(
        device0_.get(),
        kTestServiceId1,
        BluetoothUUID(kTestServiceUuid1),
        false /* is_primary */,
        false /* is_local */));

    // Assign characteristics some random properties and permissions. They don't
    // need to reflect what the characteristic is actually capable of, since
    // the JS API just passes values through from
    // device::BluetoothGattCharacteristic.
    std::vector<uint8> default_value;
    chrc0_.reset(new testing::NiceMock<MockBluetoothGattCharacteristic>(
        service0_.get(),
        kTestCharacteristicId0,
        BluetoothUUID(kTestCharacteristicUuid0),
        false /* is_local */,
        kTestCharacteristicProperties0,
        BluetoothGattCharacteristic::kPermissionNone));
    default_value.assign(kTestCharacteristicDefaultValue0,
                         (kTestCharacteristicDefaultValue0 +
                          sizeof(kTestCharacteristicDefaultValue0)));
    ON_CALL(*chrc0_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    chrc1_.reset(new testing::NiceMock<MockBluetoothGattCharacteristic>(
        service0_.get(),
        kTestCharacteristicId1,
        BluetoothUUID(kTestCharacteristicUuid1),
        false /* is_local */,
        kTestCharacteristicProperties1,
        BluetoothGattCharacteristic::kPermissionNone));
    default_value.assign(kTestCharacteristicDefaultValue1,
                         (kTestCharacteristicDefaultValue1 +
                          sizeof(kTestCharacteristicDefaultValue1)));
    ON_CALL(*chrc1_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    chrc2_.reset(new testing::NiceMock<MockBluetoothGattCharacteristic>(
        service1_.get(),
        kTestCharacteristicId2,
        BluetoothUUID(kTestCharacteristicUuid2),
        false /* is_local */,
        kTestCharacteristicProperties2,
        BluetoothGattCharacteristic::kPermissionNone));

    desc0_.reset(new testing::NiceMock<MockBluetoothGattDescriptor>(
        chrc0_.get(),
        kTestDescriptorId0,
        BluetoothUUID(kTestDescriptorUuid0),
        false /* is_local */,
        BluetoothGattCharacteristic::kPermissionNone));
    default_value.assign(
        kTestDescriptorDefaultValue0,
        (kTestDescriptorDefaultValue0 + sizeof(kTestDescriptorDefaultValue0)));
    ON_CALL(*desc0_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    desc1_.reset(new testing::NiceMock<MockBluetoothGattDescriptor>(
        chrc0_.get(),
        kTestDescriptorId1,
        BluetoothUUID(kTestDescriptorUuid1),
        false /* is_local */,
        BluetoothGattCharacteristic::kPermissionNone));
    default_value.assign(
        kTestDescriptorDefaultValue1,
        (kTestDescriptorDefaultValue1 + sizeof(kTestDescriptorDefaultValue1)));
    ON_CALL(*desc1_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));
  }

 protected:
  BluetoothLowEnergyEventRouter* event_router() {
    return extensions::BluetoothLowEnergyAPI::Get(browser()->profile())
        ->event_router();
  }

  testing::StrictMock<MockBluetoothAdapter>* mock_adapter_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device0_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device1_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattService> > service0_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattService> > service1_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattCharacteristic> > chrc0_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattCharacteristic> > chrc1_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattCharacteristic> > chrc2_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattDescriptor> > desc0_;
  scoped_ptr<testing::NiceMock<MockBluetoothGattDescriptor> > desc1_;

 private:
  scoped_refptr<extensions::Extension> empty_extension_;
};

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::std::tr1::get<k>(args).Run();
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  ::std::tr1::get<k>(args).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackWithScopedPtrArg,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  ::std::tr1::get<k>(args).Run(scoped_ptr<T>(p0));
}

BluetoothGattConnection* CreateGattConnection(
    const std::string& device_address,
    bool expect_disconnect) {
  testing::NiceMock<MockBluetoothGattConnection>* conn =
      new testing::NiceMock<MockBluetoothGattConnection>(device_address);

  if (expect_disconnect) {
    EXPECT_CALL(*conn, Disconnect(_))
        .Times(1)
        .WillOnce(InvokeCallbackArgument<0>());
  } else {
    EXPECT_CALL(*conn, Disconnect(_)).Times(0);
  }

  return conn;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetServices) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattService*> services;
  services.push_back(service0_.get());
  services.push_back(service1_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattServices())
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

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
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

  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ServiceEvents) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/service_events")));

  // Cause events to be sent to the extension.
  event_router()->DeviceAdded(mock_adapter_, device0_.get());

  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattServiceAdded(device0_.get(), service1_.get());
  event_router()->GattServiceChanged(service1_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device0_.get(), service1_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
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
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());

  ExtensionTestMessageListener get_service_success_listener("getServiceSuccess",
                                                            true);
  EXPECT_TRUE(get_service_success_listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());

  // 2. getService fail.
  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0)).Times(0);

  event_router()->GattServiceRemoved(device0_.get(), service0_.get());

  ExtensionTestMessageListener get_service_fail_listener("getServiceFail",
                                                         true);
  EXPECT_TRUE(get_service_fail_listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());

  get_service_fail_listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetIncludedServices) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_included_services")));

  // Wait for initial call to end with failure as there is no mapping.
  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Set up for the rest of the calls before replying. Included services can be
  // returned even if there is no instance ID mapping for them yet, so no need
  // to call GattServiceAdded for |service1_| here.
  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());

  std::vector<BluetoothGattService*> includes;
  includes.push_back(service1_.get());
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .Times(2)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(2)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetIncludedServices())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothGattService*>()))
      .WillOnce(Return(includes));

  listener.Reply("go");
  listener.Reset();

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristics) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattCharacteristic*> characteristics;
  characteristics.push_back(chrc0_.get());
  characteristics.push_back(chrc1_.get());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(3).WillRepeatedly(
      Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothGattService*>(NULL)))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristics())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothGattCharacteristic*>()))
      .WillOnce(Return(characteristics));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_characteristics")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(4)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothGattService*>(NULL)))
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(2)
      .WillOnce(Return(static_cast<BluetoothGattCharacteristic*>(NULL)))
      .WillOnce(Return(chrc0_.get()));

  // Load the extension and wait for first test.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_characteristic")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicProperties) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(12)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(12)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(12)
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetProperties())
      .Times(12)
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyNone))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyBroadcast))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyRead))
      .WillOnce(
           Return(BluetoothGattCharacteristic::kPropertyWriteWithoutResponse))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyWrite))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyNotify))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyIndicate))
      .WillOnce(Return(
          BluetoothGattCharacteristic::kPropertyAuthenticatedSignedWrites))
      .WillOnce(
           Return(BluetoothGattCharacteristic::kPropertyExtendedProperties))
      .WillOnce(Return(BluetoothGattCharacteristic::kPropertyReliableWrite))
      .WillOnce(
           Return(BluetoothGattCharacteristic::kPropertyWritableAuxiliaries))
      .WillOnce(Return(
          BluetoothGattCharacteristic::kPropertyBroadcast |
          BluetoothGattCharacteristic::kPropertyRead |
          BluetoothGattCharacteristic::kPropertyWriteWithoutResponse |
          BluetoothGattCharacteristic::kPropertyWrite |
          BluetoothGattCharacteristic::kPropertyNotify |
          BluetoothGattCharacteristic::kPropertyIndicate |
          BluetoothGattCharacteristic::kPropertyAuthenticatedSignedWrites |
          BluetoothGattCharacteristic::kPropertyExtendedProperties |
          BluetoothGattCharacteristic::kPropertyReliableWrite |
          BluetoothGattCharacteristic::kPropertyWritableAuxiliaries));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_properties")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_characteristic")));

  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Cause events to be sent to the extension.
  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattServiceAdded(device0_.get(), service1_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattCharacteristicAdded(service1_.get(), chrc2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(2)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId1))
      .Times(1)
      .WillOnce(Return(service1_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));
  EXPECT_CALL(*service1_, GetCharacteristic(kTestCharacteristicId2))
      .Times(1)
      .WillOnce(Return(chrc2_.get()));

  BluetoothGattNotifySession* session0 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          kTestCharacteristicId0);
  BluetoothGattNotifySession* session1 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          kTestCharacteristicId2);

  EXPECT_CALL(*chrc0_, StartNotifySession(_, _))
      .Times(1)
      .WillOnce(
          InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
              session0));
  EXPECT_CALL(*chrc2_, StartNotifySession(_, _))
      .Times(1)
      .WillOnce(
          InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
              session1));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_value_changed")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  std::vector<uint8> value;
  event_router()->GattCharacteristicValueChanged(
      service0_.get(), chrc0_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      service1_.get(), chrc2_.get(), value);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(service1_.get(), chrc2_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service1_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReadCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  std::vector<uint8> value;
  EXPECT_CALL(*chrc0_, ReadRemoteCharacteristic(_, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<1>())
      .WillOnce(InvokeCallbackArgument<0>(value));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/read_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, WriteCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  std::vector<uint8> write_value;
  EXPECT_CALL(*chrc0_, WriteRemoteCharacteristic(_, _, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<2>())
      .WillOnce(DoAll(SaveArg<0>(&write_value), InvokeCallbackArgument<1>()));

  EXPECT_CALL(*chrc0_, GetValue()).Times(1).WillOnce(ReturnRef(write_value));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/write_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptors) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattDescriptor*> descriptors;
  descriptors.push_back(desc0_.get());
  descriptors.push_back(desc1_.get());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothGattCharacteristic*>(NULL)))
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptors())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothGattDescriptor*>()))
      .WillOnce(Return(descriptors));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_descriptors")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(5)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(4)
      .WillOnce(Return(static_cast<BluetoothGattService*>(NULL)))
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothGattCharacteristic*>(NULL)))
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(2)
      .WillOnce(Return(static_cast<BluetoothGattDescriptor*>(NULL)))
      .WillOnce(Return(desc0_.get()));

  // Load the extension and wait for first test.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_descriptor")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(1)
      .WillOnce(Return(desc0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_descriptor")));

  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());
  testing::Mock::VerifyAndClearExpectations(chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);
  EXPECT_CALL(*chrc0_, GetDescriptor(_)).Times(0);

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, DescriptorValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc1_.get());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/descriptor_value_changed")));

  // Cause events to be sent to the extension.
  std::vector<uint8> value;
  event_router()->GattDescriptorValueChanged(chrc0_.get(), desc0_.get(), value);
  event_router()->GattDescriptorValueChanged(chrc0_.get(), desc1_.get(), value);

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattDescriptorRemoved(chrc0_.get(), desc1_.get());
  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReadDescriptorValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(3)
      .WillRepeatedly(Return(desc0_.get()));

  std::vector<uint8> value;
  EXPECT_CALL(*desc0_, ReadRemoteDescriptor(_, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<1>())
      .WillOnce(InvokeCallbackArgument<0>(value));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/read_descriptor_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, WriteDescriptorValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(3)
      .WillRepeatedly(Return(desc0_.get()));

  std::vector<uint8> write_value;
  EXPECT_CALL(*desc0_, WriteRemoteDescriptor(_, _, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<2>())
      .WillOnce(DoAll(SaveArg<0>(&write_value), InvokeCallbackArgument<1>()));

  EXPECT_CALL(*desc0_, GetValue()).Times(1).WillOnce(ReturnRef(write_value));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/write_descriptor_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, PermissionDenied) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/permission_denied")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, UuidPermissionMethods) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  std::vector<BluetoothGattService*> services;
  services.push_back(service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattServices()).WillOnce(Return(services));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .WillRepeatedly(Return(desc0_.get()));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/uuid_permission_methods")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, UuidPermissionEvents) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/uuid_permission_events")));

  // Cause events to be sent to the extension.
  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  std::vector<uint8> value;
  event_router()->GattCharacteristicValueChanged(
      service0_.get(), chrc0_.get(), value);
  event_router()->GattDescriptorValueChanged(chrc0_.get(), desc0_.get(), value);
  event_router()->GattServiceChanged(service0_.get());

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GattConnection) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(static_cast<BluetoothDevice*>(NULL)));
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress1))
      .WillRepeatedly(Return(device1_.get()));
  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(3)
      .WillOnce(InvokeCallbackArgument<1>(BluetoothDevice::ERROR_FAILED))
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattConnection>(
          CreateGattConnection(kTestLeDeviceAddress0,
                               true /* expect_disconnect */)))
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattConnection>(
          CreateGattConnection(kTestLeDeviceAddress0,
                               false /* expect_disconnect */)));
  EXPECT_CALL(*device1_, CreateGattConnection(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattConnection>(
          CreateGattConnection(kTestLeDeviceAddress1,
                               true /* expect_disconnect */)));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/gatt_connection")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->DeviceRemoved(mock_adapter_, device1_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReconnectAfterDisconnected) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));

  MockBluetoothGattConnection* first_conn =
      static_cast<MockBluetoothGattConnection*>(CreateGattConnection(
          kTestLeDeviceAddress0, false /* expect_disconnect */));
  EXPECT_CALL(*first_conn, IsConnected())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(2)
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattConnection>(
          first_conn))
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattConnection>(
          CreateGattConnection(kTestLeDeviceAddress0,
                               false /* expect_disconnect */)));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/reconnect_after_disconnected")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ConnectInProgress) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));

  BluetoothDevice::GattConnectionCallback connect_callback;
  base::Closure disconnect_callback;

  testing::NiceMock<MockBluetoothGattConnection>* conn =
      new testing::NiceMock<MockBluetoothGattConnection>(
          kTestLeDeviceAddress0);
  scoped_ptr<BluetoothGattConnection> conn_ptr(conn);
  EXPECT_CALL(*conn, Disconnect(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&disconnect_callback));

  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(1)
      .WillOnce(SaveArg<0>(&connect_callback));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/connect_in_progress")));

  listener.WaitUntilSatisfied();
  connect_callback.Run(conn_ptr.Pass());

  listener.Reset();
  listener.WaitUntilSatisfied();
  disconnect_callback.Run();

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, StartStopNotifications) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device0_.get());
  event_router()->GattServiceAdded(device0_.get(), service0_.get());
  event_router()->GattServiceAdded(device0_.get(), service1_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc1_.get());
  event_router()->GattCharacteristicAdded(service1_.get(), chrc2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId1))
      .WillRepeatedly(Return(service1_.get()));
  EXPECT_CALL(*service1_, GetCharacteristic(kTestCharacteristicId2))
      .Times(1)
      .WillOnce(Return(chrc2_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(2)
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId1))
      .Times(1)
      .WillOnce(Return(chrc1_.get()));

  BluetoothGattNotifySession* session0 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          kTestCharacteristicId0);
  MockBluetoothGattNotifySession* session1 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          kTestCharacteristicId1);

  EXPECT_CALL(*session1, Stop(_))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<0>());

  EXPECT_CALL(*chrc0_, StartNotifySession(_, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<1>())
      .WillOnce(
          InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
              session0));
  EXPECT_CALL(*chrc1_, StartNotifySession(_, _))
      .Times(1)
      .WillOnce(
          InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
              session1));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/start_stop_notifications")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  std::vector<uint8> value;
  event_router()->GattCharacteristicValueChanged(
      service0_.get(), chrc0_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      service0_.get(), chrc1_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      service1_.get(), chrc2_.get(), value);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(service1_.get(), chrc2_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc1_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device0_.get(), service1_.get());
  event_router()->GattServiceRemoved(device0_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device0_.get());
}

}  // namespace
