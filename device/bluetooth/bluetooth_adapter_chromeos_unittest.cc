// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::MockBluetoothAdapter;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

class BluetoothAdapterChromeOSTest : public testing::Test {
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

    callback_called_ = false;
    error_callback_called_ = false;
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  void Callback() {
    callback_called_ = true;
  }

  void ErrorCallback() {
    error_callback_called_ = true;
  }

  void SetAdapter(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

 protected:
  MockBluetoothManagerClient* mock_manager_client_;
  MockBluetoothAdapterClient* mock_adapter_client_;

  bool callback_called_;
  bool error_callback_called_;

  scoped_refptr<BluetoothAdapter> adapter_;
};

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterNotPresent) {
  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));
  ASSERT_TRUE(adapter_ != NULL);

  // Call the adapter callback; make out it failed.
  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(dbus::ObjectPath(""), false);

  // Adapter should not be present.
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterWithAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  adapter_callback.Run(adapter_path, true);

  // Adapter should be present with the given address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPresentChanged must not be called
  // yet.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Adapter should not be present yet.
  EXPECT_FALSE(adapter_->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should be present with the given address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterBecomesPresentWithAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(adapter_path);

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterReplacedWithAddress) {
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

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
      .Times(1);
  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(new_adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterBecomesPresentWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(adapter_path);

  // Adapter should not be present yet.
  EXPECT_FALSE(adapter_->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterReplacedWithoutAddress) {
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

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should be now marked not present.
  EXPECT_FALSE(adapter_->IsPresent());

  // Tell the adapter the address now;
  // BluetoothAdapter::Observer::AdapterPresentChanged now must be called.
  new_adapter_properties.address.ReplaceValue(new_adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(new_adapter_path,
                               new_adapter_properties.address.name());

  // Adapter should be present with the new address.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(new_adapter_address, adapter_->address());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterRemoved) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->AdapterRemoved(adapter_path);

  // Adapter should be no longer present.
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterWithoutAddressRemoved) {
  const dbus::ObjectPath adapter_path("/fake/hci0");

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

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
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->AdapterRemoved(adapter_path);

  // Adapter should be still no longer present.
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyInitiallyFalse) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.powered.ReplaceValue(false);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyInitiallyTrue) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterPoweredChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), true))
      .Times(1);

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyInitiallyTrueWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set but BluetoothAdapter::Observer::AdapterPoweredChanged
  // should not yet be called.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.powered.ReplaceValue(true);

  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Adapter should not yet have the property value.
  EXPECT_FALSE(adapter_->IsPowered());

  // Tell the adapter the address now,
  // BluetoothAdapter::Observer::AdapterPresentChanged and
  // BluetoothAdapter::Observer::AdapterPoweredChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), true))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterPoweredPropertyChanged) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.powered.ReplaceValue(false);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_FALSE(adapter_->IsPowered());

  // Report that the property has been changed;
  // BluetoothAdapter::Observer::AdapterPoweredChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), true))
      .Times(1);

  adapter_properties.powered.ReplaceValue(true);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.powered.name());

  // Adapter should have the new property values.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterPoweredPropertyUnchanged) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsPowered());

  // Report that the property has been changed, but don't change the value;
  // BluetoothAdapter::Observer::AdapterPoweredChanged should not be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.powered.name());

  // Adapter should still have the same property values.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyChangedWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set but BluetoothAdapter::Observer::AdapterPoweredChanged
  // should not yet be called.
  MockBluetoothAdapterClient::Properties adapter_properties;

  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Tell the adapter that its powered property changed, the observer
  // method should still not be called because there is no address for
  // the adapter so it is not present.
  adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.powered.name());

  // Adapter should not yet have the property value.
  EXPECT_FALSE(adapter_->IsPowered());

  // Tell the adapter the address now,
  // BluetoothAdapter::Observer::AdapterPresentChanged and
  // BluetoothAdapter::Observer::AdapterPoweredChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should now have the correct property value.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyResetOnReplace) {
  const dbus::ObjectPath initial_adapter_path("/fake/hci0");
  const dbus::ObjectPath new_adapter_path("/fake/hci1");
  const std::string initial_adapter_address = "CA:FE:4A:C0:FE:FE";
  const std::string new_adapter_address = "00:C0:11:CO:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);
  initial_adapter_properties.powered.ReplaceValue(true);

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

  // BluetoothAdapter::Observer::AdapterPoweredChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
        .Times(1);
  }

  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should have the new property value.
  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyResetOnReplaceWhenTrue) {
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

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);
  initial_adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(initial_adapter_path))
      .WillRepeatedly(Return(&initial_adapter_properties));

  adapter_callback.Run(initial_adapter_path, true);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties new_adapter_properties;
  new_adapter_properties.address.ReplaceValue(new_adapter_address);
  new_adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(new_adapter_path))
      .WillRepeatedly(Return(&new_adapter_properties));

  // BluetoothAdapter::Observer::AdapterPoweredChanged will be called once
  // to set the value to false for the previous adapter and once to set the
  // value to true for the new adapter.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
        .Times(1);
  }

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), true))
        .Times(1);
  }

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should have the new property value.
  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterPoweredPropertyResetOnRemove) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.powered.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Report that the adapter has been removed;
  // BluetoothAdapter::Observer::AdapterPoweredChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
      .Times(1);
  EXPECT_CALL(adapter_observer, AdapterPoweredChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->AdapterRemoved(adapter_path);

  // Adapter should have the new property value.
  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterSetPowered) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Request that the powered property be changed;
  // MockBluetoothAdapterClient::Set should be called, passing the address
  // of the powered property and a callback to receive the response.
  dbus::PropertySet::SetCallback set_callback;
  EXPECT_CALL(adapter_properties, Set(&adapter_properties.powered, _))
      .WillOnce(SaveArg<1>(&set_callback));

  adapter_->SetPowered(true,
                      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                                 base::Unretained(this)),
                      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                                 base::Unretained(this)));

  // Reply to the callback to indicate success, the set callback we provided
  // should be called but the properties should not be refetched.
  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .Times(0);

  set_callback.Run(true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterSetPoweredError) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Request that the powered property be changed;
  // MockBluetoothAdapterClient::Set should be called, passing the address
  // of the powered property and a callback to receive the response.
  dbus::PropertySet::SetCallback set_callback;
  EXPECT_CALL(adapter_properties, Set(&adapter_properties.powered, _))
      .WillOnce(SaveArg<1>(&set_callback));

  adapter_->SetPowered(true,
                       base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                                  base::Unretained(this)),
                       base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));

  // Reply to the callback to indicate failure, the error callback we provided
  // should be called but the properties should not be refetched.
  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .Times(0);

  set_callback.Run(false);

  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyInitiallyFalse) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.discovering.ReplaceValue(false);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyInitiallyTrue) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  // BluetoothAdapter::Observer::AdapterDiscoveringChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), true))
      .Times(1);

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyInitiallyTrueWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set but BluetoothAdapter::Observer::AdapterDiscoveringChanged
  // should not yet be called.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.discovering.ReplaceValue(true);

  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Adapter should not yet have the property value.
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Tell the adapter the address now,
  // BluetoothAdapter::Observer::AdapterPresentChanged and
  // BluetoothAdapter::Observer::AdapterDiscoveringChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), true))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterDiscoveringPropertyChanged) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.discovering.ReplaceValue(false);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Report that the property has been changed;
  // BluetoothAdapter::Observer::AdapterDiscoveringChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), true))
      .Times(1);

  adapter_properties.discovering.ReplaceValue(true);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.discovering.name());

  // Adapter should have the new property values.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyUnchanged) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Report that the property has been changed, but don't change the value;
  // BluetoothAdapter::Observer::AdapterDiscoveringChanged should not be
  // called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.discovering.name());

  // Adapter should still have the same property values.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyChangedWithoutAddress) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set but BluetoothAdapter::Observer::AdapterDiscoveringChanged
  // should not yet be called.
  MockBluetoothAdapterClient::Properties adapter_properties;

  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), _))
      .Times(0);

  adapter_callback.Run(adapter_path, true);

  // Tell the adapter that its discovering property changed, the observer
  // method should still not be called because there is no address for
  // the adapter so it is not present.
  adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), _))
      .Times(0);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.discovering.name());

  // Adapter should not yet have the property value.
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Tell the adapter the address now,
  // BluetoothAdapter::Observer::AdapterPresentChanged and
  // BluetoothAdapter::Observer::AdapterDiscoveringChanged now must be called.
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
      .Times(1);

  EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(), true))
      .Times(1);

  static_cast<BluetoothAdapterClient::Observer*>(adapter_chromeos)
      ->AdapterPropertyChanged(adapter_path,
                               adapter_properties.address.name());

  // Adapter should now have the correct property value.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyResetOnReplace) {
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

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);
  initial_adapter_properties.discovering.ReplaceValue(true);

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

  // BluetoothAdapter::Observer::AdapterDiscoveringChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
        .Times(1);
  }

  EXPECT_CALL(adapter_observer,
              AdapterDiscoveringChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should have the new property value.
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyResetOnReplaceWhenTrue) {
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

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties initial_adapter_properties;
  initial_adapter_properties.address.ReplaceValue(initial_adapter_address);
  initial_adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(initial_adapter_path))
      .WillRepeatedly(Return(&initial_adapter_properties));

  adapter_callback.Run(initial_adapter_path, true);

  // Tell the adapter the default adapter changed;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties new_adapter_properties;
  new_adapter_properties.address.ReplaceValue(new_adapter_address);
  new_adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(new_adapter_path))
      .WillRepeatedly(Return(&new_adapter_properties));

  // BluetoothAdapter::Observer::AdapterDiscoveringChanged will be called once
  // to set the value to false for the previous adapter and once to set the
  // value to true for the new adapter.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), true))
        .Times(1);
  }

  {
    InSequence s;

    EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(),
                                                         false))
        .Times(1);
    EXPECT_CALL(adapter_observer, AdapterDiscoveringChanged(adapter_.get(),
                                                         true))
        .Times(1);
  }

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->DefaultAdapterChanged(new_adapter_path);

  // Adapter should have the new property value.
  EXPECT_TRUE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterDiscoveringPropertyResetOnRemove) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);
  adapter_properties.discovering.ReplaceValue(true);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Report that the adapter has been removed;
  // BluetoothAdapter::Observer::AdapterDiscoveringChanged will be called.
  MockBluetoothAdapter::Observer adapter_observer;
  adapter_->AddObserver(&adapter_observer);

  EXPECT_CALL(adapter_observer, AdapterPresentChanged(adapter_.get(), false))
      .Times(1);
  EXPECT_CALL(adapter_observer,
              AdapterDiscoveringChanged(adapter_.get(), false))
      .Times(1);

  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  static_cast<BluetoothManagerClient::Observer*>(adapter_chromeos)
      ->AdapterRemoved(adapter_path);

  // Adapter should have the new property value.
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterNotInitiallyDiscovering) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Adapter should have the correct property value.
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterStartDiscovering) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return indicate success.
  start_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterStartDiscoveringError) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate failure.
  start_discovery_callback.Run(adapter_path, false);

  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterSecondStartDiscovering) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices again;
  // BluetoothAdapterClient::StartDiscovery will be called again.
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success again.
  start_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterSecondStartDiscoveringError) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices again;
  // BluetoothAdapterClient::StartDiscovery will be called again.
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate failure.
  start_discovery_callback.Run(adapter_path, false);

  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterStopDiscovering) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to stop discovering devices;
  // BluetoothAdapterClient::StopDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback stop_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  stop_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest, DefaultAdapterStopDiscoveringError) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to stop discovering devices;
  // BluetoothAdapterClient::StopDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback stop_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate failure.
  stop_discovery_callback.Run(adapter_path, false);

  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterStopDiscoveringAfterSecondStart) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices again;
  // BluetoothAdapterClient::StartDiscovery will be called again.
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to stop discovering devices;
  // BluetoothAdapterClient::StopDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback stop_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  stop_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterStopDiscoveringAfterSecondStartError) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices again;
  // BluetoothAdapterClient::StartDiscovery will be called again.
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate failure.
  start_discovery_callback.Run(adapter_path, false);

  // Ask the adapter to stop discovering devices;
  // BluetoothAdapterClient::StopDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback stop_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  stop_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

TEST_F(BluetoothAdapterChromeOSTest,
       DefaultAdapterSecondStopDiscoveringAfterSecondStart) {
  const dbus::ObjectPath adapter_path("/fake/hci0");
  const std::string adapter_address = "CA:FE:4A:C0:FE:FE";

  // Create the default adapter instance;
  // BluetoothManagerClient::DefaultAdapter will be called once, passing
  // a callback to obtain the adapter path.
  BluetoothManagerClient::AdapterCallback adapter_callback;
  EXPECT_CALL(*mock_manager_client_, DefaultAdapter(_))
      .WillOnce(SaveArg<0>(&adapter_callback));

  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterChromeOSTest::SetAdapter,
                 base::Unretained(this)));

  // Call the adapter callback;
  // BluetoothAdapterClient::GetProperties will be called once to obtain
  // the property set.
  MockBluetoothAdapterClient::Properties adapter_properties;
  adapter_properties.address.ReplaceValue(adapter_address);

  EXPECT_CALL(*mock_adapter_client_, GetProperties(adapter_path))
      .WillRepeatedly(Return(&adapter_properties));

  adapter_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices;
  // BluetoothAdapterClient::StartDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback start_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to start discovering devices again;
  // BluetoothAdapterClient::StartDiscovery will be called again.
  EXPECT_CALL(*mock_adapter_client_, StartDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&start_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StartDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  start_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to stop discovering devices;
  // BluetoothAdapterClient::StopDiscovery will be called.
  BluetoothAdapterClient::AdapterCallback stop_discovery_callback;
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  stop_discovery_callback.Run(adapter_path, true);

  // Ask the adapter to stop discovering devices again;
  // BluetoothAdapterClient::StopDiscovery will be called.
  EXPECT_CALL(*mock_adapter_client_, StopDiscovery(adapter_path, _))
      .WillOnce(SaveArg<1>(&stop_discovery_callback));

  callback_called_ = false;
  error_callback_called_ = false;

  adapter_->StopDiscovering(
      base::Bind(&BluetoothAdapterChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Return to indicate success.
  stop_discovery_callback.Run(adapter_path, true);

  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(error_callback_called_);
}

}  // namespace chromeos
