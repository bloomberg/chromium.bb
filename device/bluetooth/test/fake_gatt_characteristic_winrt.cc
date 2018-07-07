// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDescriptor;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattPresentationFormat;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattProtectionLevel;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadClientCharacteristicConfigurationDescriptorResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattValueChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Storage::Streams::IBuffer;

}  // namespace

FakeGattCharacteristicWinrt::FakeGattCharacteristicWinrt() = default;

FakeGattCharacteristicWinrt::~FakeGattCharacteristicWinrt() = default;

HRESULT FakeGattCharacteristicWinrt::GetDescriptors(
    GUID descriptor_uuid,
    IVectorView<GattDescriptor*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_CharacteristicProperties(
    GattCharacteristicProperties* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_ProtectionLevel(
    GattProtectionLevel* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::put_ProtectionLevel(
    GattProtectionLevel value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_UserDescription(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_Uuid(GUID* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_AttributeHandle(uint16_t* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_PresentationFormats(
    IVectorView<GattPresentationFormat*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::ReadValueAsync(
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::ReadValueWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueAsync(
    IBuffer* value,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueWithOptionAsync(
    IBuffer* value,
    GattWriteOption write_option,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::
    ReadClientCharacteristicConfigurationDescriptorAsync(
        IAsyncOperation<
            GattReadClientCharacteristicConfigurationDescriptorResult*>**
            async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::
    WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue
            client_characteristic_configuration_descriptor_value,
        IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::add_ValueChanged(
    ITypedEventHandler<GattCharacteristic*, GattValueChangedEventArgs*>*
        value_changed_handler,
    EventRegistrationToken* value_changed_event_cookie) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::remove_ValueChanged(
    EventRegistrationToken value_changed_event_cookie) {
  return E_NOTIMPL;
}

}  // namespace device
