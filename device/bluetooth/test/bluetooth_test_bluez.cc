// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_bluez.h"

#include <iterator>
#include <sstream>

#include "base/logging.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"

namespace device {

namespace {

void AdapterCallback(base::Closure quit_closure) {
  quit_closure.Run();
}
}

BluetoothTestBlueZ::BluetoothTestBlueZ()
    : fake_bluetooth_device_client_(nullptr) {}

BluetoothTestBlueZ::~BluetoothTestBlueZ() {}

void BluetoothTestBlueZ::SetUp() {
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
}
