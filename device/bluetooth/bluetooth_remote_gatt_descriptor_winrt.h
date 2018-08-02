// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/client.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorWinrt
    : public BluetoothRemoteGattDescriptor {
 public:
  static std::unique_ptr<BluetoothRemoteGattDescriptorWinrt> Create(
      BluetoothRemoteGattCharacteristic* characteristic,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDescriptor>
          descriptor);
  ~BluetoothRemoteGattDescriptorWinrt() override;

  // BluetoothGattDescriptor:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;

  // BluetoothRemoteGattDescriptor:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(const ValueCallback& callback,
                            const ErrorCallback& error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& value,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;

 private:
  BluetoothRemoteGattDescriptorWinrt(
      BluetoothRemoteGattCharacteristic* characteristic,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDescriptor>
          descriptor,
      BluetoothUUID uuid,
      uint16_t attribute_handle);

  // Weak. This object is owned by |characteristic_|.
  BluetoothRemoteGattCharacteristic* characteristic_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                             GenericAttributeProfile::IGattDescriptor>
      descriptor_;
  BluetoothUUID uuid_;
  std::string identifier_;
  std::vector<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
