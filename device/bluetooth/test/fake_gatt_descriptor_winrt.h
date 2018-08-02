// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/implements.h>

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"

namespace device {

class FakeGattDescriptorWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDescriptor,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDescriptor2> {
 public:
  FakeGattDescriptorWinrt(base::StringPiece uuid, uint16_t attribute_handle);
  ~FakeGattDescriptorWinrt() override;

  // IGattDescriptor:
  IFACEMETHODIMP get_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel* value) override;
  IFACEMETHODIMP put_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;
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
              GattCommunicationStatus>** action) override;

  // IGattDescriptor2:
  IFACEMETHODIMP WriteValueWithResultAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattWriteResult*>** operation) override;

 private:
  GUID uuid_;
  uint16_t attribute_handle_;

  DISALLOW_COPY_AND_ASSIGN(FakeGattDescriptorWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_
