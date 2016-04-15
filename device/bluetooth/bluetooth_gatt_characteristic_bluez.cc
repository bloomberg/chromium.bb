// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_characteristic_bluez.h"

#include "device/bluetooth/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluetooth_gatt_service_bluez.h"

namespace bluez {

BluetoothGattCharacteristicBlueZ::BluetoothGattCharacteristicBlueZ(
    BluetoothGattServiceBlueZ* service,
    const dbus::ObjectPath& object_path)
    : service_(service), object_path_(object_path), weak_ptr_factory_(this) {}

BluetoothGattCharacteristicBlueZ::~BluetoothGattCharacteristicBlueZ() {}

std::string BluetoothGattCharacteristicBlueZ::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothGattService* BluetoothGattCharacteristicBlueZ::GetService()
    const {
  return service_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothGattCharacteristicBlueZ::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return PERMISSION_NONE;
}

std::vector<device::BluetoothGattDescriptor*>
BluetoothGattCharacteristicBlueZ::GetDescriptors() const {
  std::vector<device::BluetoothGattDescriptor*> descriptors;
  for (DescriptorMap::const_iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    descriptors.push_back(iter->second);
  return descriptors;
}

device::BluetoothGattDescriptor*
BluetoothGattCharacteristicBlueZ::GetDescriptor(
    const std::string& identifier) const {
  DescriptorMap::const_iterator iter =
      descriptors_.find(dbus::ObjectPath(identifier));
  if (iter == descriptors_.end())
    return nullptr;
  return iter->second;
}

}  // namespace bluez
