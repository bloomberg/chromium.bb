// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_ANDROID_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

namespace device {

// BluetoothRemoteGattCharacteristicAndroid along with its owned Java class
// org.chromium.device.bluetooth.ChromeBluetoothRemoteGattCharacteristic
// implement BluetootGattCharacteristic.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicAndroid
    : public BluetoothGattCharacteristic {
 public:
  // Create a BluetoothRemoteGattServiceAndroid instance and associated Java
  // ChromeBluetoothRemoteGattService using the provided
  // |bluetooth_remote_gatt_service_wrapper|.
  //
  // The ChromeBluetoothRemoteGattService instance will hold a Java reference
  // to |bluetooth_remote_gatt_service_wrapper|.
  //
  // TODO(scheib): Actually create the Java object. crbug.com/545682
  static scoped_ptr<BluetoothRemoteGattCharacteristicAndroid> Create();

  ~BluetoothRemoteGattCharacteristicAndroid() override;

  // BluetoothGattCharacteristic interface:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  const std::vector<uint8>& GetValue() const override;
  BluetoothGattService* GetService() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;
  bool IsNotifying() const override;
  std::vector<BluetoothGattDescriptor*> GetDescriptors() const override;
  BluetoothGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  bool AddDescriptor(BluetoothGattDescriptor* descriptor) override;
  bool UpdateValue(const std::vector<uint8>& value) override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

 private:
  BluetoothRemoteGattCharacteristicAndroid();

  std::vector<uint8> value_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_ANDROID_H_
