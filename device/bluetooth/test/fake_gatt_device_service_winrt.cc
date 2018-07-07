// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_device_service_winrt.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattOpenStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattSession;
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;

}  // namespace

FakeGattDeviceServiceWinrt::FakeGattDeviceServiceWinrt(
    uint16_t attribute_handle,
    base::StringPiece uuid)
    : attribute_handle_(attribute_handle),
      uuid_(BluetoothUUID::GetCanonicalValueAsGUID(uuid)) {}

FakeGattDeviceServiceWinrt::~FakeGattDeviceServiceWinrt() = default;

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristics(
    GUID characteristic_uuid,
    IVectorView<GattCharacteristic*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServices(
    GUID service_uuid,
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_DeviceId(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_Uuid(GUID* value) {
  *value = uuid_;
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::get_AttributeHandle(uint16_t* value) {
  *value = attribute_handle_;
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::get_DeviceAccessInformation(
    IDeviceAccessInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_Session(IGattSession** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_SharingMode(GattSharingMode* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::RequestAccessAsync(
    IAsyncOperation<DeviceAccessStatus>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::OpenAsync(
    GattSharingMode sharing_mode,
    IAsyncOperation<GattOpenStatus>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsAsync(
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsForUuidAsync(
    GUID characteristic_uuid,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsForUuidWithCacheModeAsync(
    GUID characteristic_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesAsync(
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesForUuidAsync(
    GUID service_uuid,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT
FakeGattDeviceServiceWinrt::GetIncludedServicesForUuidWithCacheModeAsync(
    GUID service_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

}  // namespace device
