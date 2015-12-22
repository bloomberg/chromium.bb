// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattCharacteristic;

}  // namespace device

namespace bluez {

class BluetoothRemoteGattCharacteristicBlueZ;

// The BluetoothRemoteGattDescriptorBlueZ class implements
// BluetoothGattDescriptor for remote GATT characteristic descriptors on the
// Chrome OS platform.
class BluetoothRemoteGattDescriptorBlueZ
    : public device::BluetoothGattDescriptor {
 public:
  // device::BluetoothGattDescriptor overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothGattCharacteristic* GetCharacteristic() const override;
  device::BluetoothGattCharacteristic::Permissions GetPermissions()
      const override;
  void ReadRemoteDescriptor(const ValueCallback& callback,
                            const ErrorCallback& error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class BluetoothRemoteGattCharacteristicBlueZ;

  BluetoothRemoteGattDescriptorBlueZ(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      const dbus::ObjectPath& object_path);
  ~BluetoothRemoteGattDescriptorBlueZ() override;

  // Called by dbus:: on unsuccessful completion of a request to read or write
  // the descriptor value.
  void OnError(const ErrorCallback& error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // Object path of the D-Bus descriptor object.
  dbus::ObjectPath object_path_;

  // The GATT characteristic this descriptor belongs to.
  BluetoothRemoteGattCharacteristicBlueZ* characteristic_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattDescriptorBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_
