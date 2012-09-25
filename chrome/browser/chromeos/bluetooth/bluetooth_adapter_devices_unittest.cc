// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/test/mock_bluetooth_adapter.h"
#include "chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chromeos/dbus/mock_bluetooth_device_client.h"
#include "chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

class BluetoothAdapterDevicesTest : public testing::Test {
 public:
  virtual void SetUp() {
    MockDBusThreadManager* mock_dbus_thread_manager = new MockDBusThreadManager;

    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);

    mock_manager_client_ =
        mock_dbus_thread_manager->mock_bluetooth_manager_client();
    mock_adapter_client_ =
        mock_dbus_thread_manager->mock_bluetooth_adapter_client();
    mock_device_client_ =
        mock_dbus_thread_manager->mock_bluetooth_device_client();

    // Create the default adapter instance;
    // BluetoothManagerClient::DefaultAdapter will be called once, passing
    // a callback to obtain the adapter path.
    BluetoothManagerClient::AdapterCallback adapter_callback;
    EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
        .WillOnce(SaveArg<0>(&adapter_callback));

    EXPECT_CALL(*mock_manager_client_, AddObserver(_))
        .Times(1);
    EXPECT_CALL(*mock_adapter_client_, AddObserver(_))
        .Times(1);

    adapter_ = BluetoothAdapter::DefaultAdapter();

    // Call the adapter callback;
    // BluetoothAdapterClient::GetProperties will be called once to obtain
    // the property set.
    MockBluetoothAdapterClient::Properties adapter_properties;
    adapter_properties.address.ReplaceValue(adapter_address_);
    adapter_properties.powered.ReplaceValue(true);

    EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path_))
        .WillRepeatedly(Return(&adapter_properties));

    // Add an observer to the adapter; expect the usual set of changes to
    // an adapter becoming present and then clear to clean up for the test.
    adapter_->AddObserver(&adapter_observer_);

    EXPECT_CALL(adapter_observer_, AdapterPresentChanged(adapter_.get(), true))
        .Times(1);
    EXPECT_CALL(adapter_observer_, AdapterPoweredChanged(adapter_.get(), true))
        .Times(1);

    adapter_callback.Run(adapter_path_, true);

    Mock::VerifyAndClearExpectations(mock_manager_client_);
    Mock::VerifyAndClearExpectations(mock_adapter_client_);
    Mock::VerifyAndClearExpectations(mock_device_client_);
    Mock::VerifyAndClearExpectations(&adapter_observer_);
  }

  virtual void TearDown() {
    EXPECT_CALL(*mock_device_client_, RemoveObserver(adapter_.get()))
        .Times(1);
    EXPECT_CALL(*mock_adapter_client_, RemoveObserver(adapter_.get()))
        .Times(1);
    EXPECT_CALL(*mock_manager_client_, RemoveObserver(adapter_.get()))
        .Times(1);

    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

 protected:
  MockBluetoothManagerClient* mock_manager_client_;
  MockBluetoothAdapterClient* mock_adapter_client_;
  MockBluetoothDeviceClient* mock_device_client_;

  static const dbus::ObjectPath adapter_path_;
  static const std::string adapter_address_;
  scoped_refptr<BluetoothAdapter> adapter_;

  MockBluetoothAdapter::Observer adapter_observer_;
};

const dbus::ObjectPath BluetoothAdapterDevicesTest::adapter_path_(
    "/fake/hci0");
const std::string BluetoothAdapterDevicesTest::adapter_address_ =
    "CA:FE:4A:C0:FE:FE";

TEST_F(BluetoothAdapterDevicesTest, DeviceRemovedAfterFound) {
  const dbus::ObjectPath device_path("/fake/hci0/dev_ba_c0_11_00_00_01");
  const std::string device_address = "BA:C0:11:00:00:01";

  MockBluetoothDeviceClient::Properties device_properties;
  device_properties.address.ReplaceValue(device_address);
  device_properties.name.ReplaceValue("Fake Keyboard");
  device_properties.bluetooth_class.ReplaceValue(0x2540);

  // needs to be a supported class

  // should create a bluetooth device,
  // put it in the hash map,
  // set visible to true,
  // update its properties,

  // Inform the adapter that the device has been found;
  // BluetoothAdapterClient::Observer::DeviceAdded will be called, passing
  // the device object.
  BluetoothDevice* device;
  EXPECT_CALL(adapter_observer_, DeviceAdded(adapter_.get(), _))
      .Times(1)
      .WillOnce(SaveArg<1>(&device));

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceFound(adapter_path_, device_address, device_properties);

  // Now inform the adapter that the device has been added and assigned an
  // object path; BluetoothDeviceClient::GetProperties will be called to
  // obtain the property set; and
  // BluetoothAdapterClient::Observer::DeviceChanged will be called passing
  // the same device object as before.
  EXPECT_CALL(*mock_device_client_, GetProperties(device_path))
      .WillRepeatedly(Return(&device_properties));

  EXPECT_CALL(adapter_observer_, DeviceChanged(adapter_.get(), device))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceCreated(adapter_path_, device_path);

  // Finally remove the adapter again; since this is a supported device
  // BluetoothAdapterClient::Observer::DeviceRemoved should be not called,
  // instead BluetoothAdapterClient::Observer::DeviceChanged will be called.
  EXPECT_CALL(adapter_observer_, DeviceRemoved(adapter_.get(), device))
      .Times(0);
  EXPECT_CALL(adapter_observer_, DeviceChanged(adapter_.get(), device))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceRemoved(adapter_path_, device_path);

  // Verify that the device is still visible, just no longer paired.
  EXPECT_TRUE(device->IsVisible());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothAdapterDevicesTest, UnsupportedDeviceRemovedAfterFound) {
  const dbus::ObjectPath device_path("/fake/hci0/dev_ba_c0_11_00_00_02");
  const std::string device_address = "BA:C0:11:00:00:02";

  MockBluetoothDeviceClient::Properties device_properties;
  device_properties.address.ReplaceValue(device_address);
  device_properties.name.ReplaceValue("Fake Computer");
  device_properties.bluetooth_class.ReplaceValue(0x400100);

  // Inform the adapter that the unsupported device has been found;
  // BluetoothAdapterClient::Observer::DeviceAdded should not be called
  // yet because this device is not supported so is hidden from the UI.
  EXPECT_CALL(adapter_observer_, DeviceAdded(adapter_.get(), _))
      .Times(0);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceFound(adapter_path_, device_address, device_properties);

  // Now inform the adapter the device has been added and assigned an
  // object path; BluetoothDeviceClient::GetProperties will be called
  // to obtain the property set; and
  // BluetoothAdapterClient::Observer::DeviceAdded will be called,
  // passing the device object.
  EXPECT_CALL(*mock_device_client_, GetProperties(device_path))
      .WillRepeatedly(Return(&device_properties));

  BluetoothDevice* device;
  EXPECT_CALL(adapter_observer_, DeviceAdded(adapter_.get(), _))
      .Times(1)
      .WillOnce(SaveArg<1>(&device));

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceCreated(adapter_path_, device_path);

  // Finally remove the device again;
  // BluetoothAdapterClient::Observer::DeviceRemoved will be called
  // before the device object is deleted.
  EXPECT_CALL(adapter_observer_, DeviceRemoved(adapter_.get(), device))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_.get())
      ->DeviceRemoved(adapter_path_, device_path);
}

}  // namespace chromeos
