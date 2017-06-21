// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_characteristic.h"

#include <utility>
#include <vector>

#include "base/optional.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/public/interfaces/test/fake_bluetooth.mojom.h"

namespace bluetooth {

FakeRemoteGattCharacteristic::FakeRemoteGattCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid,
    mojom::CharacteristicPropertiesPtr properties,
    device::BluetoothRemoteGattService* service)
    : characteristic_id_(characteristic_id),
      characteristic_uuid_(characteristic_uuid),
      service_(service),
      weak_ptr_factory_(this) {
  properties_ = PROPERTY_NONE;
  if (properties->broadcast)
    properties_ |= PROPERTY_BROADCAST;
  if (properties->read)
    properties_ |= PROPERTY_READ;
  if (properties->write_without_response)
    properties_ |= PROPERTY_WRITE_WITHOUT_RESPONSE;
  if (properties->write)
    properties_ |= PROPERTY_WRITE;
  if (properties->notify)
    properties_ |= PROPERTY_NOTIFY;
  if (properties->indicate)
    properties_ |= PROPERTY_INDICATE;
  if (properties->authenticated_signed_writes)
    properties_ |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  if (properties->extended_properties)
    properties_ |= PROPERTY_EXTENDED_PROPERTIES;
}

FakeRemoteGattCharacteristic::~FakeRemoteGattCharacteristic() {}

void FakeRemoteGattCharacteristic::SetNextReadResponse(
    uint16_t gatt_code,
    const base::Optional<std::vector<uint8_t>>& value) {
  DCHECK(!next_read_response_);
  next_read_response_.emplace(gatt_code, value);
}

std::string FakeRemoteGattCharacteristic::GetIdentifier() const {
  return characteristic_id_;
}

device::BluetoothUUID FakeRemoteGattCharacteristic::GetUUID() const {
  return characteristic_uuid_;
}

FakeRemoteGattCharacteristic::Properties
FakeRemoteGattCharacteristic::GetProperties() const {
  return properties_;
}

FakeRemoteGattCharacteristic::Permissions
FakeRemoteGattCharacteristic::GetPermissions() const {
  NOTREACHED();
  return PERMISSION_NONE;
}

const std::vector<uint8_t>& FakeRemoteGattCharacteristic::GetValue() const {
  NOTREACHED();
  return value_;
}

device::BluetoothRemoteGattService* FakeRemoteGattCharacteristic::GetService()
    const {
  return service_;
}

std::vector<device::BluetoothRemoteGattDescriptor*>
FakeRemoteGattCharacteristic::GetDescriptors() const {
  NOTREACHED();
  return std::vector<device::BluetoothRemoteGattDescriptor*>();
}

device::BluetoothRemoteGattDescriptor*
FakeRemoteGattCharacteristic::GetDescriptor(
    const std::string& identifier) const {
  NOTREACHED();
  return nullptr;
}

void FakeRemoteGattCharacteristic::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeRemoteGattCharacteristic::DispatchReadResponse,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
}

void FakeRemoteGattCharacteristic::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeRemoteGattCharacteristic::SubscribeToNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeRemoteGattCharacteristic::UnsubscribeFromNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakeRemoteGattCharacteristic::DispatchReadResponse(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(next_read_response_);
  uint16_t gatt_code = next_read_response_->gatt_code;
  base::Optional<std::vector<uint8_t>> value =
      std::move(next_read_response_->value);
  next_read_response_.reset();

  if (gatt_code == mojom::kGATTSuccess) {
    DCHECK(value);
    value_ = std::move(value.value());
    callback.Run(value_);
    return;
  } else if (gatt_code == mojom::kGATTInvalidHandle) {
    DCHECK(!value);
    error_callback.Run(device::BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }
}

FakeRemoteGattCharacteristic::ReadResponse::ReadResponse(
    uint16_t gatt_code,
    const base::Optional<std::vector<uint8_t>>& value)
    : gatt_code(gatt_code), value(value) {}

FakeRemoteGattCharacteristic::ReadResponse::~ReadResponse() {}

}  // namespace bluetooth
