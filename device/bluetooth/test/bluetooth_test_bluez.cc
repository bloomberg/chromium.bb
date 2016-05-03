// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_bluez.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"

namespace device {

namespace {

void AdapterCallback(const base::Closure& quit_closure) {
  quit_closure.Run();
}

void GetValueCallback(
    const base::Closure& quit_closure,
    const BluetoothLocalGattService::Delegate::ValueCallback& value_callback,
    const std::vector<uint8_t>& value) {
  value_callback.Run(value);
  quit_closure.Run();
}

void ClosureCallback(const base::Closure& quit_closure,
                     const base::Closure& callback) {
  callback.Run();
  quit_closure.Run();
}

}  // namespace

BluetoothTestBlueZ::BluetoothTestBlueZ()
    : fake_bluetooth_device_client_(nullptr) {}

BluetoothTestBlueZ::~BluetoothTestBlueZ() {}

void BluetoothTestBlueZ::SetUp() {
  BluetoothTestBase::SetUp();
  std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
      bluez::BluezDBusManager::GetSetterForTesting();
  fake_bluetooth_device_client_ = new bluez::FakeBluetoothDeviceClient;
  // TODO(rkc): This is a big hacky. Creating a device client creates three
  // devices by default. For now, the easiest path is to just clear them, but
  // a better way will be to only create them as needed. This will require
  // looking at a lot of tests but should be done eventually.
  fake_bluetooth_device_client_->RemoveAllDevices();
  dbus_setter->SetBluetoothDeviceClient(
      std::unique_ptr<bluez::BluetoothDeviceClient>(
          fake_bluetooth_device_client_));
}

void BluetoothTestBlueZ::TearDown() {
  adapter_ = nullptr;
  bluez::BluezDBusManager::Shutdown();
  BluetoothTestBase::TearDown();
}

bool BluetoothTestBlueZ::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestBlueZ::InitWithFakeAdapter() {
  base::RunLoop run_loop;
  adapter_ = new bluez::BluetoothAdapterBlueZ(
      base::Bind(&AdapterCallback, run_loop.QuitClosure()));
  run_loop.Run();
}

BluetoothDevice* BluetoothTestBlueZ::DiscoverLowEnergyDevice(
    int device_ordinal) {
  if (device_ordinal > 4 || device_ordinal < 1)
    return nullptr;

  std::string device_name = kTestDeviceName;
  std::string device_address = kTestDeviceAddress1;
  std::vector<std::string> service_uuids;

  switch (device_ordinal) {
    case 1:
      service_uuids.push_back(kTestUUIDGenericAccess);
      service_uuids.push_back(kTestUUIDGenericAttribute);
      break;
    case 2:
      service_uuids.push_back(kTestUUIDImmediateAlert);
      service_uuids.push_back(kTestUUIDLinkLoss);
      break;
    case 3:
      device_name = kTestDeviceNameEmpty;
      break;
    case 4:
      device_name = kTestDeviceNameEmpty;
      device_address = kTestDeviceAddress2;
      break;
  }

  if (!adapter_->GetDevice(device_address)) {
    fake_bluetooth_device_client_->CreateTestDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        device_name /* name */, device_name /* alias */, device_address,
        service_uuids);
  }
  BluetoothDevice* device = adapter_->GetDevice(device_address);

  return device;
}

void BluetoothTestBlueZ::SimulateLocalGattCharacteristicValueReadRequest(
    BluetoothLocalGattService* service,
    BluetoothLocalGattCharacteristic* characteristic,
    const BluetoothLocalGattService::Delegate::ValueCallback& value_callback,
    const base::Closure& error_callback) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service);
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  base::RunLoop run_loop;
  characteristic_provider->GetValue(
      base::Bind(&GetValueCallback, run_loop.QuitClosure(), value_callback),
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), error_callback));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattCharacteristicValueWriteRequest(
    BluetoothLocalGattService* service,
    BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value_to_write,
    const base::Closure& success_callback,
    const base::Closure& error_callback) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service);
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  base::RunLoop run_loop;
  characteristic_provider->SetValue(
      value_to_write,
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), success_callback),
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), error_callback));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattDescriptorValueReadRequest(
    BluetoothLocalGattService* service,
    BluetoothLocalGattDescriptor* descriptor,
    const BluetoothLocalGattService::Delegate::ValueCallback& value_callback,
    const base::Closure& error_callback) {
  bluez::BluetoothLocalGattDescriptorBlueZ* descriptor_bluez =
      static_cast<bluez::BluetoothLocalGattDescriptorBlueZ*>(descriptor);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattDescriptorServiceProvider* descriptor_provider =
      fake_bluetooth_gatt_manager_client->GetDescriptorServiceProvider(
          descriptor_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service);
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_descriptor(descriptor);

  base::RunLoop run_loop;
  descriptor_provider->GetValue(
      base::Bind(&GetValueCallback, run_loop.QuitClosure(), value_callback),
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), error_callback));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattDescriptorValueWriteRequest(
    BluetoothLocalGattService* service,
    BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value_to_write,
    const base::Closure& success_callback,
    const base::Closure& error_callback) {
  bluez::BluetoothLocalGattDescriptorBlueZ* descriptor_bluez =
      static_cast<bluez::BluetoothLocalGattDescriptorBlueZ*>(descriptor);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattDescriptorServiceProvider* descriptor_provider =
      fake_bluetooth_gatt_manager_client->GetDescriptorServiceProvider(
          descriptor_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service);
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_descriptor(descriptor);

  base::RunLoop run_loop;
  descriptor_provider->SetValue(
      value_to_write,
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), success_callback),
      base::Bind(&ClosureCallback, run_loop.QuitClosure(), error_callback));
  run_loop.Run();
}

}  // namespace device
