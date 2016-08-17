// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/arc_bluetooth_bridge.h"

#include <string>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/arc/bluetooth/bluetooth_type_converters.h"
#include "components/arc/common/bluetooth.mojom.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/arc/test/fake_bluetooth_instance.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcBluetoothBridgeTest : public testing::Test {
 protected:
  void AddTestDevice() {
    bluez::BluezDBusManager* dbus_manager = bluez::BluezDBusManager::Get();
    auto* fake_bluetooth_device_client =
        static_cast<bluez::FakeBluetoothDeviceClient*>(
            dbus_manager->GetBluetoothDeviceClient());
    auto* fake_bluetooth_gatt_service_client =
        static_cast<bluez::FakeBluetoothGattServiceClient*>(
            dbus_manager->GetBluetoothGattServiceClient());
    auto* fake_bluetooth_gatt_characteristic_client =
        static_cast<bluez::FakeBluetoothGattCharacteristicClient*>(
            dbus_manager->GetBluetoothGattCharacteristicClient());

    fake_bluetooth_device_client->CreateDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    fake_bluetooth_gatt_service_client->ExposeHeartRateService(
        dbus::ObjectPath(bluez::FakeBluetoothDeviceClient::kLowEnergyPath));
    fake_bluetooth_gatt_characteristic_client->ExposeHeartRateCharacteristics(
        fake_bluetooth_gatt_service_client->GetHeartRateServicePath());
  }

  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
    get_adapter_run_loop_.Quit();
  }

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    auto fake_bluetooth_device_client =
        base::MakeUnique<bluez::FakeBluetoothDeviceClient>();
    fake_bluetooth_device_client->RemoveAllDevices();
    dbus_setter->SetBluetoothDeviceClient(
        std::move(fake_bluetooth_device_client));
    dbus_setter->SetBluetoothGattServiceClient(
        base::MakeUnique<bluez::FakeBluetoothGattServiceClient>());
    dbus_setter->SetBluetoothGattCharacteristicClient(
        base::MakeUnique<bluez::FakeBluetoothGattCharacteristicClient>());
    dbus_setter->SetBluetoothGattDescriptorClient(
        base::MakeUnique<bluez::FakeBluetoothGattDescriptorClient>());

    fake_arc_bridge_service_.reset(new FakeArcBridgeService());
    fake_bluetooth_instance_.reset(new FakeBluetoothInstance());
    fake_arc_bridge_service_->bluetooth()->SetInstance(
        fake_bluetooth_instance_.get(), 2);
    arc_bluetooth_bridge_.reset(
        new ArcBluetoothBridge(fake_arc_bridge_service_.get()));

    device::BluetoothAdapterFactory::GetAdapter(base::Bind(
        &ArcBluetoothBridgeTest::OnAdapterInitialized, base::Unretained(this)));
    // We will quit the loop once we get the adapter.
    get_adapter_run_loop_.Run();
  }

  std::unique_ptr<FakeArcBridgeService> fake_arc_bridge_service_;
  std::unique_ptr<FakeBluetoothInstance> fake_bluetooth_instance_;
  std::unique_ptr<ArcBluetoothBridge> arc_bluetooth_bridge_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::MessageLoop message_loop_;
  base::RunLoop get_adapter_run_loop_;
};

// When we add device to bluez::FakeBluetoothDeviceClient, ArcBluetoothBridge
// should send new device data to Android. This test will then check
// the correctness of the device properties sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, DeviceFound) {
  EXPECT_EQ(0u, fake_bluetooth_instance_->device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(1u, fake_bluetooth_instance_->device_found_data().size());
  const mojo::Array<mojom::BluetoothPropertyPtr>& prop =
      fake_bluetooth_instance_->device_found_data().back();

  EXPECT_EQ(7u, prop.size());
  EXPECT_TRUE(prop[0]->is_bdname());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            prop[0]->get_bdname());
  EXPECT_TRUE(prop[1]->is_bdaddr());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            prop[1]->get_bdaddr()->To<std::string>());
  EXPECT_TRUE(prop[2]->is_uuids());
  EXPECT_EQ(1u, prop[2]->get_uuids().size());
  EXPECT_EQ(bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID,
            prop[2]->get_uuids()[0].To<device::BluetoothUUID>().value());
  EXPECT_TRUE(prop[3]->is_device_class());
  EXPECT_EQ(bluez::FakeBluetoothDeviceClient::kLowEnergyClass,
            prop[3]->get_device_class());
  EXPECT_TRUE(prop[4]->is_device_type());
  // bluez::FakeBluetoothDeviceClient does not return proper device type.
  EXPECT_TRUE(prop[5]->is_remote_friendly_name());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            prop[5]->get_remote_friendly_name());
  EXPECT_TRUE(prop[6]->is_remote_rssi());
}

// Invoke OnDiscoveryStarted to send cached device to BT instance,
// and check correctness of the Advertising data sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, LEDeviceFound) {
  EXPECT_EQ(0u, fake_bluetooth_instance_->device_found_data().size());
  AddTestDevice();
  EXPECT_EQ(1u, fake_bluetooth_instance_->le_device_found_data().size());

  const mojom::BluetoothAddressPtr& addr =
      fake_bluetooth_instance_->le_device_found_data().back()->addr();
  const mojo::Array<mojom::BluetoothAdvertisingDataPtr>& adv_data =
      fake_bluetooth_instance_->le_device_found_data().back()->adv_data();

  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());
  EXPECT_EQ(2u, adv_data.size());

  EXPECT_TRUE(adv_data[0]->is_local_name());
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyName),
            adv_data[0]->get_local_name().To<std::string>());

  EXPECT_TRUE(adv_data[1]->is_service_uuids_16());
  EXPECT_EQ(1u, adv_data[1]->get_service_uuids_16().size());

  std::string uuid16_str =
      base::StringPrintf("%04x", adv_data[1]->get_service_uuids_16()[0]);
  EXPECT_EQ(bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID,
            device::BluetoothUUID(uuid16_str).canonical_value());
}

// Invoke GetGattDB and check correctness of the GattDB sent via arc bridge.
TEST_F(ArcBluetoothBridgeTest, GetGattDB) {
  AddTestDevice();

  arc_bluetooth_bridge_->GetGattDB(mojom::BluetoothAddress::From(
      std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress)));
  EXPECT_EQ(1u, fake_bluetooth_instance_->gatt_db_result().size());

  const mojom::BluetoothAddressPtr& addr =
      fake_bluetooth_instance_->gatt_db_result().back()->remote_addr();
  EXPECT_EQ(std::string(bluez::FakeBluetoothDeviceClient::kLowEnergyAddress),
            addr->To<std::string>());

  // HeartRateService in bluez::FakeBluetoothDeviceClient consists of
  // Service: HeartRateService
  //     Characteristic: HeartRateMeasurement
  //         Descriptor: ClientCharacteristicConfiguration
  //     Characteristic: BodySensorLocation
  //     Characteristic: HeartRateControlPoint
  const mojo::Array<mojom::BluetoothGattDBElementPtr>& db =
      fake_bluetooth_instance_->gatt_db_result().back()->db();
  EXPECT_EQ(5u, db.size());

  EXPECT_EQ(device::BluetoothUUID(
                bluez::FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
            db[0]->uuid.To<device::BluetoothUUID>());
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_PRIMARY_SERVICE,
            db[0]->type);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kHeartRateMeasurementUUID),
            db[1]->uuid.To<device::BluetoothUUID>());
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[1]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_NOTIFY,
            db[1]->properties);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattDescriptorClient::
                                      kClientCharacteristicConfigurationUUID),
            db[2]->uuid.To<device::BluetoothUUID>());
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_DESCRIPTOR,
            db[2]->type);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kBodySensorLocationUUID),
            db[3]->uuid.To<device::BluetoothUUID>());
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[3]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_READ,
            db[3]->properties);

  EXPECT_EQ(device::BluetoothUUID(bluez::FakeBluetoothGattCharacteristicClient::
                                      kHeartRateControlPointUUID),
            db[4]->uuid.To<device::BluetoothUUID>());
  EXPECT_EQ(mojom::BluetoothGattDBAttributeType::BTGATT_DB_CHARACTERISTIC,
            db[4]->type);
  EXPECT_EQ(device::BluetoothGattCharacteristic::PROPERTY_WRITE,
            db[4]->properties);
}

}  // namespace arc
