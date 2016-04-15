// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <ostream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

namespace {

// Stream operator for logging vector<uint8_t>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8_t> bytes) {
  out << "[";
  for (std::vector<uint8_t>::const_iterator iter = bytes.begin();
       iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothGattDescriptorBlueZ::BluetoothGattDescriptorBlueZ(
    BluetoothGattCharacteristicBlueZ* characteristic,
    const dbus::ObjectPath& object_path,
    bool is_local)
    : object_path_(object_path),
      characteristic_(characteristic),
      is_local_(is_local),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT descriptor with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
}

BluetoothGattDescriptorBlueZ::~BluetoothGattDescriptorBlueZ() {}

std::string BluetoothGattDescriptorBlueZ::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID BluetoothGattDescriptorBlueZ::GetUUID() const {
  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothGattDescriptorBlueZ::IsLocal() const {
  return is_local_;
}

const std::vector<uint8_t>& BluetoothGattDescriptorBlueZ::GetValue() const {
  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path_);

  DCHECK(properties);

  return properties->value.value();
}

device::BluetoothGattCharacteristic*
BluetoothGattDescriptorBlueZ::GetCharacteristic() const {
  return characteristic_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothGattDescriptorBlueZ::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return device::BluetoothGattCharacteristic::PERMISSION_NONE;
}

void BluetoothGattDescriptorBlueZ::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic descriptor read request to "
          << "descriptor: " << GetIdentifier()
          << ", UUID: " << GetUUID().canonical_value();

  bluez::BluezDBusManager::Get()->GetBluetoothGattDescriptorClient()->ReadValue(
      object_path_, callback,
      base::Bind(&BluetoothGattDescriptorBlueZ::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothGattDescriptorBlueZ::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic descriptor write request to "
          << "characteristic: " << GetIdentifier()
          << ", UUID: " << GetUUID().canonical_value()
          << ", with value: " << new_value << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->WriteValue(object_path_, new_value, callback,
                   base::Bind(&BluetoothGattDescriptorBlueZ::OnError,
                              weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothGattDescriptorBlueZ::OnError(const ErrorCallback& error_callback,
                                           const std::string& error_name,
                                           const std::string& error_message) {
  VLOG(1) << "Operation failed: " << error_name
          << ", message: " << error_message;

  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
