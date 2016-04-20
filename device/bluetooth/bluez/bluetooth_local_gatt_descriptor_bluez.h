// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"

namespace bluez {

// The BluetoothLocalGattDescriptorBlueZ class implements
// BluetoothRemoteGattDescriptor for remote and local GATT characteristic
// descriptors
// for platforms that use BlueZ.
class BluetoothLocalGattDescriptorBlueZ
    : public BluetoothGattDescriptorBlueZ,
      public device::BluetoothLocalGattDescriptor {
 public:
 private:
  explicit BluetoothLocalGattDescriptorBlueZ(
      const dbus::ObjectPath& object_path);
  ~BluetoothLocalGattDescriptorBlueZ() override;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattDescriptorBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattDescriptorBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_
