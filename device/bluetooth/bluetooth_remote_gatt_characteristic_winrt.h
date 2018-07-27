// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicWinrt
    : public BluetoothRemoteGattCharacteristic {
 public:
  static std::unique_ptr<BluetoothRemoteGattCharacteristicWinrt> Create(
      BluetoothRemoteGattService* service,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattCharacteristic* characteristic);
  ~BluetoothRemoteGattCharacteristicWinrt() override;

  // BluetoothGattCharacteristic:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // BluetoothRemoteGattCharacteristic:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

 protected:
  // BluetoothRemoteGattCharacteristic:
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* ccc_descriptor,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback) override;
  void UnsubscribeFromNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

 private:
  BluetoothRemoteGattCharacteristicWinrt(BluetoothRemoteGattService* service,
                                         BluetoothUUID uuid,
                                         Properties proporties,
                                         uint16_t attribute_handle);

  BluetoothRemoteGattService* service_;
  BluetoothUUID uuid_;
  Properties properties_;
  std::string identifier_;
  std::vector<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WINRT_H_
