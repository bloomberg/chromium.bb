// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

class BluetoothUUID;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorWinrt
    : public BluetoothRemoteGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorWinrt();
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
  std::vector<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
