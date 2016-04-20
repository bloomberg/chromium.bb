// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"

namespace device {

class BluetoothAdapter;
class BluetoothDevice;

}  // namespace device

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothDeviceBlueZ;

// The BluetoothLocalGattServiceBlueZ class implements BluetootGattService
// for local GATT services for platforms that use BlueZ.
class BluetoothLocalGattServiceBlueZ
    : public BluetoothGattServiceBlueZ,
      public device::BluetoothLocalGattService {
 public:
  // device::BluetoothLocalGattService overrides.
  void Register(const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void Unregister(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;

 private:
  friend class BluetoothDeviceBlueZ;

  BluetoothLocalGattServiceBlueZ(BluetoothAdapterBlueZ* adapter,
                                 const dbus::ObjectPath& object_path);
  ~BluetoothLocalGattServiceBlueZ() override;

  // Called by dbus:: on unsuccessful completion of a request to register a
  // local service.
  void OnRegistrationError(const ErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattServiceBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattServiceBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_
