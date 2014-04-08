// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattService;
using device::BluetoothUUID;

namespace chromeos {

namespace {

class TestObserver : public BluetoothDevice::Observer {
 public:
  TestObserver(scoped_refptr<BluetoothAdapter> adapter,
                     BluetoothDevice* device)
      : gatt_service_added_count_(0),
        gatt_service_removed_count_(0),
        device_address_(device->GetAddress()),
        adapter_(adapter) {
    device->AddObserver(this);
  }

  virtual ~TestObserver() {
    BluetoothDevice* device = adapter_->GetDevice(device_address_);
    if (device)
      device->RemoveObserver(this);
  }

  // BluetoothDevice::Observer overrides.
  virtual void GattServiceAdded(
      BluetoothDevice* device,
      BluetoothGattService* service) OVERRIDE {
    EXPECT_EQ(device_address_, device->GetAddress());

    ++gatt_service_added_count_;
    last_gatt_service_id_ = service->GetIdentifier();
    last_gatt_service_uuid_ = service->GetUUID();

    EXPECT_FALSE(service->IsLocal());
    EXPECT_TRUE(service->IsPrimary());

    QuitMessageLoop();
  }

  virtual void GattServiceRemoved(
      BluetoothDevice* device,
      BluetoothGattService* service) OVERRIDE {
    EXPECT_EQ(device_address_, device->GetAddress());

    ++gatt_service_removed_count_;
    last_gatt_service_id_ = service->GetIdentifier();
    last_gatt_service_uuid_ = service->GetUUID();

    EXPECT_FALSE(service->IsLocal());
    EXPECT_TRUE(service->IsPrimary());

    QuitMessageLoop();
  }

  int gatt_service_added_count_;
  int gatt_service_removed_count_;
  std::string last_gatt_service_id_;
  BluetoothUUID last_gatt_service_uuid_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  std::string device_address_;
  scoped_refptr<BluetoothAdapter> adapter_;
};

}  // namespace

class BluetoothGattChromeOSTest : public testing::Test {
 public:
  BluetoothGattChromeOSTest()
      : fake_bluetooth_gatt_service_client_(NULL),
        last_gatt_service_(NULL) {
  }

  virtual void SetUp() {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    fake_bluetooth_gatt_service_client_ =
        new FakeBluetoothGattServiceClient;
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(
            fake_bluetooth_device_client_));
    fake_dbus_thread_manager->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            fake_bluetooth_gatt_service_client_));
    fake_dbus_thread_manager->SetBluetoothGattCharacteristicClient(
        scoped_ptr<BluetoothGattCharacteristicClient>(
            new FakeBluetoothGattCharacteristicClient));
    fake_dbus_thread_manager->SetBluetoothGattDescriptorClient(
        scoped_ptr<BluetoothGattDescriptorClient>(
            new FakeBluetoothGattDescriptorClient));
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothGattChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    adapter_->SetPowered(
        true,
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

 protected:
  base::MessageLoop message_loop_;

  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  FakeBluetoothGattServiceClient* fake_bluetooth_gatt_service_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  BluetoothGattService* last_gatt_service_;
};

TEST_F(BluetoothGattChromeOSTest, GattServiceAdded) {
  // Create a fake LE device. We store the device pointer here because this is a
  // test. It's unsafe to do this in production as the device might get deleted.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestObserver observer(adapter_, device);
  EXPECT_EQ(0, observer.gatt_service_added_count_);
  EXPECT_EQ(0, observer.gatt_service_removed_count_);
  EXPECT_TRUE(observer.last_gatt_service_id_.empty());
  EXPECT_FALSE(observer.last_gatt_service_uuid_.IsValid());

  // Expose the fake Heart Rate Service.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.gatt_service_added_count_);
  EXPECT_EQ(0, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id_);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(observer.last_gatt_service_uuid_, service->GetUUID());

  // Hide the service.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_gatt_service_client_->HideHeartRateService();

  EXPECT_EQ(1, observer.gatt_service_added_count_);
  EXPECT_EQ(1, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_TRUE(device->GetGattServices().empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  EXPECT_EQ(NULL, device->GetGattService(observer.last_gatt_service_id_));

  // Expose the service again.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(2, observer.gatt_service_added_count_);
  EXPECT_EQ(1, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);

  service = device->GetGattService(observer.last_gatt_service_id_);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(observer.last_gatt_service_uuid_, service->GetUUID());

  // Remove the device. The observer should be notified of the removed service.
  // |device| becomes invalid after this.
  observer.last_gatt_service_uuid_ = BluetoothUUID();
  observer.last_gatt_service_id_.clear();
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  EXPECT_EQ(2, observer.gatt_service_added_count_);
  EXPECT_EQ(2, observer.gatt_service_removed_count_);
  EXPECT_FALSE(observer.last_gatt_service_id_.empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid_);
  EXPECT_EQ(
      NULL, adapter_->GetDevice(FakeBluetoothDeviceClient::kLowEnergyAddress));
}

}  // namespace chromeos
