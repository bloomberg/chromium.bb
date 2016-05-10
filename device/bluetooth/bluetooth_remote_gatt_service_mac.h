// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_

#include <stdint.h>

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

@class CBService;

namespace device {

class BluetoothDevice;
class BluetoothGattCharacteristic;
class BluetoothLowEnergyDeviceMac;
class BluetoothTestMac;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceMac
    : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceMac(
      BluetoothLowEnergyDeviceMac* bluetooth_device_mac,
      CBService* service,
      bool is_primary);
  ~BluetoothRemoteGattServiceMac() override;

  // BluetoothRemoteGattService override.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;

 private:
  friend BluetoothLowEnergyDeviceMac;
  friend BluetoothTestMac;

  // Returns CBService.
  CBService* GetService() const;

  // bluetooth_device_mac_ owns instances of this class.
  BluetoothLowEnergyDeviceMac* bluetooth_device_mac_;
  // A service from CBPeripheral.services.
  base::scoped_nsobject<CBService> service_;
  bool is_primary_;
  // Service identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_
