// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_descriptor_winrt.h"

#include "base/strings/string_piece.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattProtectionLevel;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Storage::Streams::IBuffer;

}  // namespace

FakeGattDescriptorWinrt::FakeGattDescriptorWinrt(base::StringPiece uuid,
                                                 uint16_t attribute_handle)
    : uuid_(BluetoothUUID::GetCanonicalValueAsGUID(uuid)),
      attribute_handle_(attribute_handle) {}

FakeGattDescriptorWinrt::~FakeGattDescriptorWinrt() = default;

HRESULT FakeGattDescriptorWinrt::get_ProtectionLevel(
    GattProtectionLevel* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::put_ProtectionLevel(
    GattProtectionLevel value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::get_Uuid(GUID* value) {
  *value = uuid_;
  return S_OK;
}

HRESULT FakeGattDescriptorWinrt::get_AttributeHandle(uint16_t* value) {
  *value = attribute_handle_;
  return S_OK;
}

HRESULT FakeGattDescriptorWinrt::ReadValueAsync(
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::ReadValueWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::WriteValueAsync(
    IBuffer* value,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::WriteValueWithResultAsync(
    IBuffer* value,
    IAsyncOperation<GattWriteResult*>** operation) {
  return E_NOTIMPL;
}

}  // namespace device
