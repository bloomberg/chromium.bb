// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDescriptor;
using Microsoft::WRL::ComPtr;

}  // namespace

// static
std::unique_ptr<BluetoothRemoteGattDescriptorWinrt>
BluetoothRemoteGattDescriptorWinrt::Create(
    BluetoothRemoteGattCharacteristic* characteristic,
    ComPtr<IGattDescriptor> descriptor) {
  DCHECK(descriptor);
  GUID guid;
  HRESULT hr = descriptor->get_Uuid(&guid);
  if (FAILED(hr)) {
    VLOG(2) << "Getting UUID failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  uint16_t attribute_handle;
  hr = descriptor->get_AttributeHandle(&attribute_handle);
  if (FAILED(hr)) {
    VLOG(2) << "Getting AttributeHandle failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new BluetoothRemoteGattDescriptorWinrt(
      characteristic, std::move(descriptor), BluetoothUUID(guid),
      attribute_handle));
}

BluetoothRemoteGattDescriptorWinrt::~BluetoothRemoteGattDescriptorWinrt() =
    default;

std::string BluetoothRemoteGattDescriptorWinrt::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattDescriptorWinrt::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorWinrt::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorWinrt::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorWinrt::GetCharacteristic() const {
  return characteristic_;
}

void BluetoothRemoteGattDescriptorWinrt::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattDescriptorWinrt::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothRemoteGattDescriptorWinrt::BluetoothRemoteGattDescriptorWinrt(
    BluetoothRemoteGattCharacteristic* characteristic,
    Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                               GenericAttributeProfile::IGattDescriptor>
        descriptor,
    BluetoothUUID uuid,
    uint16_t attribute_handle)
    : characteristic_(characteristic),
      descriptor_(std::move(descriptor)),
      uuid_(std::move(uuid)),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     characteristic_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle)) {}

}  // namespace device
