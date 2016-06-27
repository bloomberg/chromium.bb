// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include "base/mac/scoped_nsobject.h"

@class CBCharacteristic;

namespace device {

class BluetoothRemoteGattServiceMac;

// The BluetoothRemoteGattCharacteristicMac class implements
// BluetoothRemoteGattCharacteristic for remote GATT services on OS X.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicMac
    : public BluetoothRemoteGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicMac(
      BluetoothRemoteGattServiceMac* gatt_service,
      CBCharacteristic* cb_characteristic);
  ~BluetoothRemoteGattCharacteristicMac() override;

  // Override BluetoothGattCharacteristic methods.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // Override BluetoothRemoteGattCharacteristic methods.
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  bool IsNotifying() const override;
  std::vector<BluetoothRemoteGattDescriptor*> GetDescriptors() const override;
  BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicMac);

 private:
  friend class BluetoothRemoteGattServiceMac;
  friend class BluetoothTestMac;

  // Called by the BluetoothRemoteGattServiceMac instance when the
  // characteristics value has been read.
  void DidUpdateValue(NSError* error);
  // Returns true if the characteristic is readable.
  bool IsReadable() const;
  // Returns CoreBluetooth characteristic.
  CBCharacteristic* GetCBCharacteristic() const;

  // gatt_service_ owns instances of this class.
  BluetoothRemoteGattServiceMac* gatt_service_;
  // A characteristic from CBPeripheral.services.characteristics.
  base::scoped_nsobject<CBCharacteristic> cb_characteristic_;
  // Characteristic identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;
  // Characteristic value.
  std::vector<uint8_t> value_;
  // True if a gatt read or write request is in progress.
  bool characteristic_value_read_or_write_in_progress_;
  // ReadRemoteCharacteristic request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_characteristic_value_callbacks_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_
