// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace device {

// static
base::WeakPtr<device::BluetoothLocalGattCharacteristic>
BluetoothLocalGattCharacteristic::Create(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions,
    device::BluetoothLocalGattService* service) {
  DCHECK(service);
  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(service);
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic =
      new bluez::BluetoothLocalGattCharacteristicBlueZ(uuid, service_bluez);
  return characteristic->weak_ptr_factory_.GetWeakPtr();
}

}  // device

namespace bluez {

BluetoothLocalGattCharacteristicBlueZ::BluetoothLocalGattCharacteristicBlueZ(
    const device::BluetoothUUID& uuid,
    BluetoothLocalGattServiceBlueZ* service)
    : BluetoothGattCharacteristicBlueZ(
          BluetoothLocalGattServiceBlueZ::AddGuidToObjectPath(
              service->object_path().value() + "/characteristic")),
      uuid_(uuid),
      service_(service),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating local GATT characteristic with identifier: "
          << GetIdentifier();
  service->AddCharacteristic(base::WrapUnique(this));
}

BluetoothLocalGattCharacteristicBlueZ::
    ~BluetoothLocalGattCharacteristicBlueZ() {}

device::BluetoothUUID BluetoothLocalGattCharacteristicBlueZ::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicBlueZ::GetProperties() const {
  NOTIMPLEMENTED();
  return Properties();
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattCharacteristicBlueZ::GetPermissions() const {
  NOTIMPLEMENTED();
  return Permissions();
}

BluetoothLocalGattServiceBlueZ*
BluetoothLocalGattCharacteristicBlueZ::GetService() {
  return service_;
}

void BluetoothLocalGattCharacteristicBlueZ::AddDescriptor(
    std::unique_ptr<BluetoothLocalGattDescriptorBlueZ> descriptor) {
  descriptors_.push_back(std::move(descriptor));
}

const std::vector<std::unique_ptr<BluetoothLocalGattDescriptorBlueZ>>&
BluetoothLocalGattCharacteristicBlueZ::GetDescriptors() const {
  return descriptors_;
}

}  // namespace bluez
