// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattCharacteristic;

}  // namespace device

namespace chromeos {

class BluetoothRemoteGattCharacteristicChromeOS;

// The BluetoothRemoteGattDescriptorChromeOS class implements
// BluetoothGattDescriptor for remote GATT characteristic descriptors on the
// Chrome OS platform.
class BluetoothRemoteGattDescriptorChromeOS
    : public device::BluetoothGattDescriptor {
 public:
  // device::BluetoothGattDescriptor overrides.
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual device::BluetoothUUID GetUUID() const OVERRIDE;
  virtual bool IsLocal() const OVERRIDE;
  virtual const std::vector<uint8>& GetValue() const OVERRIDE;
  virtual device::BluetoothGattCharacteristic*
      GetCharacteristic() const OVERRIDE;
  virtual device::BluetoothGattCharacteristic::Permissions
      GetPermissions() const OVERRIDE;
  virtual void ReadRemoteDescriptor(
      const ValueCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void WriteRemoteDescriptor(
      const std::vector<uint8>& new_value,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class BluetoothRemoteGattCharacteristicChromeOS;

  BluetoothRemoteGattDescriptorChromeOS(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic,
      const dbus::ObjectPath& object_path);
  virtual ~BluetoothRemoteGattDescriptorChromeOS();

  // Called by dbus:: on completion of the request to get the descriptor value.
  void OnGetValue(const ValueCallback& callback,
                  const ErrorCallback& error_callback,
                  bool success);

  // Called by dbus:: on completion of the request to set the descriptor value.
  void OnSetValue(const base::Closure& callback,
                  const ErrorCallback& error_callback,
                  bool success);

  // Object path of the D-Bus descriptor object.
  dbus::ObjectPath object_path_;

  // The GATT characteristic this descriptor belongs to.
  BluetoothRemoteGattCharacteristicChromeOS* characteristic_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattDescriptorChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CHROMEOS_H_
