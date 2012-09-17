// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/test/mock_bluetooth_adapter.h"
#include "chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

class BluetoothAdapterTest : public testing::Test {
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
  }

  virtual void TearDown() {
    DBusThreadManager::Shutdown();
  }

 protected:
  MockBluetoothManagerClient* mock_manager_client_;
  MockBluetoothAdapterClient* mock_adapter_client_;
};

TEST_F(BluetoothAdapterTest, DefaultAdapterNotPresent) {
  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback; make out it failed.
  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), _))
      .Times(0);

  adapter_callback.Run(dbus::ObjectPath(""), false);

  // Adapter should not be present.
  EXPECT_FALSE(adapter->IsPresent());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterWithAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged will be called to
  // indicate the adapter is now present.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  adapter_callback.Run(adapter_path, true);

  // Adapter should be present with the given address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called
  // yet.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Adapter should not be present yet.
  EXPECT_FALSE(adapter->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter.get())
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should be present with the given address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterBecomesPresentWithAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback; make out it failed.
  adapter_callback.Run(dbus::ObjectPath(""), false);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->DefaultAdapterChanged(adapter_path);

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterReplacedWithAddress) {
  const dbus::ObjectPath initial_adapter_path("/fake/hci0");
  const dbus::ObjectPath new_adapter_path("/fake/hci1");
  const std::string initial_adapter_address = "CA:FE:4A:C0:FE:FE";
  const std::string new_adapter_address = "BA:C0:11:CO:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(initial_adapter_path))
      .WillRepeatedly(Return(&initial_adapter_properties));

  adapter_callback.Run(initial_adapter_path, true);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties new_adapter_properties;
  new_adapter_properties.address.ReplaceValue(new_adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(new_adapter_path))
      .WillRepeatedly(Return(&new_adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must be called once
  // with false to indicate the old adapter being removed and once with true
  // to announce the new adapter.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), false))
      .Times(1);
  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(new_adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterBecomesPresentWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback; make out it failed.
  adapter_callback.Run(dbus::ObjectPath(""), false);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), _))
      .Times(0);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->DefaultAdapterChanged(adapter_path);

  // Adapter should not be present yet.
  EXPECT_FALSE(adapter->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter.get())
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterReplacedWithoutAddress) {
  const dbus::ObjectPath initial_adapter_path("/fake/hci0");
  const dbus::ObjectPath new_adapter_path("/fake/hci1");
  const std::string initial_adapter_address = "CA:FE:4A:C0:FE:FE";
  const std::string new_adapter_address = "BA:C0:11:CO:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(initial_adapter_path))
      .WillRepeatedly(Return(&initial_adapter_properties));

  adapter_callback.Run(initial_adapter_path, true);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties new_adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(new_adapter_path))
      .WillRepeatedly(Return(&new_adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must be called to
  // indicate the adapter has gone away.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), false))
      .Times(1);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should be now marked not present.
  EXPECT_FALSE(adapter->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  new_adapter_properties.address.ReplaceValue(new_adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter.get())
      ->AdapterPropertyChanged(new_adapter_path,
                               new_adapter_properties.address.name());

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter->IsPresent());
  EXPECT_EQ(new_adapter_address, adapter->address());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterRemoved) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Report that the adapter has been removed;
  // BluetoothAdapter::Observer::AdapterPresentChanged will be called to
  // indicate the adapter is no longer present.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), false))
      .Times(1);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->AdapterRemoved(adapter_path);

  // Adapter should be no longer present.
  EXPECT_FALSE(adapter->IsPresent());
}

TEST_F(BluetoothAdapterTest, DefaultAdapterWithoutAddressRemoved) {
  const dbus::ObjectPath adapter_path("/fake/hci0");

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  scoped_refptr<BluetoothAdapter> adapter = BluetoothAdapter::DefaultAdapter();

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Report that the adapter has been removed;
  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called
  // since we never should have announced it in the first place.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter.get(), _))
      .Times(0);

  static_cast<BluetoothManagerClient::Observer*>(adapter.get())
      ->AdapterRemoved(adapter_path);

  // Adapter should be still no longer present.
  EXPECT_FALSE(adapter->IsPresent());
}

}  // namespace chromeos
