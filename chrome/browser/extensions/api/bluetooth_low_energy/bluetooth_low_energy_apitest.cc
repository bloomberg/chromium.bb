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
#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothUUID;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothGattCharacteristic;
using device::MockBluetoothGattDescriptor;
using device::MockBluetoothGattService;
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
const char kTestLeDeviceAddress[] = "11:22:33:44:55:66";
const char kTestLeDeviceName[] = "Test LE Device";

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
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device_;
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

void ReadValueSuccessCallback(
    const base::Callback<void(const std::vector<uint8>&)>& callback,
    const base::Closure& error_callback) {
  std::vector<uint8> value;
  callback.Run(value);
}

void ReadValueErrorCallback(
    const base::Callback<void(const std::vector<uint8>&)>& callback,
    const base::Closure& error_callback) {
  error_callback.Run();
}

void WriteValueSuccessCallback(const std::vector<uint8>& value,
                               const base::Closure& callback,
                               const base::Closure& error_callback) {
  callback.Run();
}

void WriteValueErrorCallback(const std::vector<uint8>& value,
                             const base::Closure& callback,
                             const base::Closure& error_callback) {
  error_callback.Run();
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
  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());

  std::vector<BluetoothGattService*> includes;
  includes.push_back(service1_.get());
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress))
      .Times(2)
      .WillRepeatedly(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristics) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattCharacteristic*> characteristics;
  characteristics.push_back(chrc0_.get());
  characteristics.push_back(chrc1_.get());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(3).WillRepeatedly(
      Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(4)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicProperties) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(12)
      .WillRepeatedly(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
           Return(BluetoothGattCharacteristic::kPropertyWriteableAuxiliaries))
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
          BluetoothGattCharacteristic::kPropertyWriteableAuxiliaries));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_properties")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_characteristic")));

  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_value_changed")));

  // Cause events to be sent to the extension.
  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattServiceAdded(device_.get(), service1_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattCharacteristicAdded(service1_.get(), chrc2_.get());

  std::vector<uint8> value;
  event_router()->GattCharacteristicValueChanged(
      service0_.get(), chrc0_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      service1_.get(), chrc2_.get(), value);

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(service1_.get(), chrc2_.get());
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device_.get(), service1_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReadCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, ReadRemoteCharacteristic(_, _))
      .Times(2)
      .WillOnce(Invoke(&ReadValueErrorCallback))
      .WillOnce(Invoke(&ReadValueSuccessCallback));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/read_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, WriteCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  std::vector<uint8> write_value;
  EXPECT_CALL(*chrc0_, WriteRemoteCharacteristic(_, _, _))
      .Times(2)
      .WillOnce(Invoke(&WriteValueErrorCallback))
      .WillOnce(
          DoAll(SaveArg<0>(&write_value), Invoke(&WriteValueSuccessCallback)));
  EXPECT_CALL(*chrc0_, GetValue()).Times(1).WillOnce(ReturnRef(write_value));

  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/write_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptors) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  std::vector<BluetoothGattDescriptor*> descriptors;
  descriptors.push_back(desc0_.get());
  descriptors.push_back(desc1_.get());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(5)
      .WillOnce(Return(static_cast<BluetoothDevice*>(NULL)))
      .WillRepeatedly(Return(device_.get()));

  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device_.get()));
  EXPECT_CALL(*device_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(1)
      .WillOnce(Return(desc0_.get()));

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(service0_.get(), chrc0_.get());
  event_router()->GattDescriptorAdded(chrc0_.get(), desc0_.get());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_descriptor")));

  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());
  testing::Mock::VerifyAndClearExpectations(chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);
  EXPECT_CALL(*chrc0_, GetDescriptor(_)).Times(0);

  event_router()->GattDescriptorRemoved(chrc0_.get(), desc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(service0_.get(), chrc0_.get());
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, DescriptorValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  event_router()->DeviceAdded(mock_adapter_, device_.get());
  event_router()->GattServiceAdded(device_.get(), service0_.get());
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
  event_router()->GattServiceRemoved(device_.get(), service0_.get());
  event_router()->DeviceRemoved(mock_adapter_, device_.get());
}

}  // namespace
