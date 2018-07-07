// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.foundation.collections.h>
#include <wrl/implements.h>

#include <stdint.h>

#include "base/macros.h"

namespace device {

class FakeGattCharacteristicWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattCharacteristic> {
 public:
  FakeGattCharacteristicWinrt();
  ~FakeGattCharacteristicWinrt() override;

  // IGattCharacteristic:
  IFACEMETHODIMP GetDescriptors(
      GUID descriptor_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptor*>** value) override;
  IFACEMETHODIMP get_CharacteristicProperties(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristicProperties* value) override;
  IFACEMETHODIMP get_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel* value) override;
  IFACEMETHODIMP put_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel value) override;
  IFACEMETHODIMP get_UserDescription(HSTRING* value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;
  IFACEMETHODIMP get_PresentationFormats(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattPresentationFormat*>** value) override;
  IFACEMETHODIMP ReadValueAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP ReadValueWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP WriteValueAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP WriteValueWithOptionAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteOption
          write_option,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP ReadClientCharacteristicConfigurationDescriptorAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadClientCharacteristicConfigurationDescriptorResult*>**
          async_op) override;
  IFACEMETHODIMP WriteClientCharacteristicConfigurationDescriptorAsync(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattClientCharacteristicConfigurationDescriptorValue
              client_characteristic_configuration_descriptor_value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP add_ValueChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristic*,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattValueChangedEventArgs*>* value_changed_handler,
      EventRegistrationToken* value_changed_event_cookie) override;
  IFACEMETHODIMP remove_ValueChanged(
      EventRegistrationToken value_changed_event_cookie) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGattCharacteristicWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_
