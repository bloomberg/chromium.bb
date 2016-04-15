// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_

#include <stdint.h>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;

}  // namespace device

namespace bluez {

class BluetoothLocalGattDescriptorBlueZ;
class BluetoothLocalGattServiceBlueZ;

// The BluetoothLocalGattCharacteristicBlueZ class implements
// BluetoothGattCharacteristic for remote GATT characteristics for platforms
// that use BlueZ.
class BluetoothLocalGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ {
 public:
  // device::BluetoothGattCharacteristic overrides.
  device::BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  const std::vector<uint8_t>& GetValue() const override;
  Properties GetProperties() const override;
  bool IsNotifying() const override;
  bool AddDescriptor(device::BluetoothGattDescriptor* descriptor) override;
  bool UpdateValue(const std::vector<uint8_t>& value) override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

 private:
  friend class BluetoothLocalGattServiceBlueZ;

  BluetoothLocalGattCharacteristicBlueZ(BluetoothLocalGattServiceBlueZ* service,
                                        const dbus::ObjectPath& object_path);
  ~BluetoothLocalGattCharacteristicBlueZ() override;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
