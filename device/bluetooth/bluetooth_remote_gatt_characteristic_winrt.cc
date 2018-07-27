// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_winrt.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic;

}  // namespace

// static
std::unique_ptr<BluetoothRemoteGattCharacteristicWinrt>
BluetoothRemoteGattCharacteristicWinrt::Create(
    BluetoothRemoteGattService* service,
    IGattCharacteristic* characteristic) {
  DCHECK(characteristic);
  GUID guid;
  HRESULT hr = characteristic->get_Uuid(&guid);
  if (FAILED(hr)) {
    VLOG(2) << "Getting UUID failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  GattCharacteristicProperties properties;
  hr = characteristic->get_CharacteristicProperties(&properties);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Properties failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  uint16_t attribute_handle;
  hr = characteristic->get_AttributeHandle(&attribute_handle);
  if (FAILED(hr)) {
    VLOG(2) << "Getting AttributeHandle failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new BluetoothRemoteGattCharacteristicWinrt(
      service, BluetoothUUID(guid), properties, attribute_handle));
}

BluetoothRemoteGattCharacteristicWinrt::
    ~BluetoothRemoteGattCharacteristicWinrt() = default;

std::string BluetoothRemoteGattCharacteristicWinrt::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicWinrt::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicWinrt::GetProperties() const {
  return properties_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicWinrt::GetPermissions() const {
  NOTIMPLEMENTED();
  return Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicWinrt::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicWinrt::GetService()
    const {
  return service_;
}

void BluetoothRemoteGattCharacteristicWinrt::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicWinrt::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicWinrt::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicWinrt::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothRemoteGattCharacteristicWinrt::BluetoothRemoteGattCharacteristicWinrt(
    BluetoothRemoteGattService* service,
    BluetoothUUID uuid,
    Properties properties,
    uint16_t attribute_handle)
    : service_(service),
      uuid_(std::move(uuid)),
      properties_(properties),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     service_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle)) {}

}  // namespace device
