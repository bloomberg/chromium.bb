// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service_bluez.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluetooth_gatt_characteristic_bluez.h"

namespace bluez {

namespace {

// TODO(jamuraa) move these to cros_system_api later
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorInvalidValueLength[] = "org.bluez.Error.InvalidValueLength";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotPaired[] = "org.bluez.Error.NotPaired";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";

}  // namespace

BluetoothGattServiceBlueZ::BluetoothGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const dbus::ObjectPath& object_path)
    : adapter_(adapter), object_path_(object_path) {
  DCHECK(adapter_);
}

BluetoothGattServiceBlueZ::~BluetoothGattServiceBlueZ() {}

std::vector<device::BluetoothGattCharacteristic*>
BluetoothGattServiceBlueZ::GetCharacteristics() const {
  std::vector<device::BluetoothGattCharacteristic*> characteristics;
  for (CharacteristicMap::const_iterator iter = characteristics_.begin();
       iter != characteristics_.end(); ++iter) {
    characteristics.push_back(iter->second);
  }
  return characteristics;
}

std::string BluetoothGattServiceBlueZ::GetIdentifier() const {
  return object_path_.value();
}

std::vector<device::BluetoothGattService*>
BluetoothGattServiceBlueZ::GetIncludedServices() const {
  // TODO(armansito): Return the actual included services here.
  return std::vector<device::BluetoothGattService*>();
}

device::BluetoothGattCharacteristic*
BluetoothGattServiceBlueZ::GetCharacteristic(
    const std::string& identifier) const {
  CharacteristicMap::const_iterator iter =
      characteristics_.find(dbus::ObjectPath(identifier));
  if (iter == characteristics_.end())
    return nullptr;
  return iter->second;
}

// static
device::BluetoothGattService::GattErrorCode
BluetoothGattServiceBlueZ::DBusErrorToServiceError(std::string error_name) {
  device::BluetoothGattService::GattErrorCode code = GATT_ERROR_UNKNOWN;
  if (error_name == kErrorFailed) {
    code = GATT_ERROR_FAILED;
  } else if (error_name == kErrorInProgress) {
    code = GATT_ERROR_IN_PROGRESS;
  } else if (error_name == kErrorInvalidValueLength) {
    code = GATT_ERROR_INVALID_LENGTH;
  } else if (error_name == kErrorNotPermitted) {
    code = GATT_ERROR_NOT_PERMITTED;
  } else if (error_name == kErrorNotAuthorized) {
    code = GATT_ERROR_NOT_AUTHORIZED;
  } else if (error_name == kErrorNotPaired) {
    code = GATT_ERROR_NOT_PAIRED;
  } else if (error_name == kErrorNotSupported) {
    code = GATT_ERROR_NOT_SUPPORTED;
  }
  return code;
}

BluetoothAdapterBlueZ* BluetoothGattServiceBlueZ::GetAdapter() const {
  return adapter_;
}

}  // namespace bluez
