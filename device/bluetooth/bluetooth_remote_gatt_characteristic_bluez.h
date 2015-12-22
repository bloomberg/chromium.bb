// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;

}  // namespace device

namespace bluez {

class BluetoothRemoteGattDescriptorBlueZ;
class BluetoothRemoteGattServiceBlueZ;

// The BluetoothRemoteGattCharacteristicBlueZ class implements
// BluetoothGattCharacteristic for remote GATT characteristics on the Chrome OS
// platform.
class BluetoothRemoteGattCharacteristicBlueZ
    : public device::BluetoothGattCharacteristic,
      public bluez::BluetoothGattDescriptorClient::Observer {
 public:
  // device::BluetoothGattCharacteristic overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothGattService* GetService() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;
  bool IsNotifying() const override;
  std::vector<device::BluetoothGattDescriptor*> GetDescriptors() const override;
  device::BluetoothGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  bool AddDescriptor(device::BluetoothGattDescriptor* descriptor) override;
  bool UpdateValue(const std::vector<uint8_t>& value) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;

  // Removes one value update session and invokes |callback| on completion. This
  // decrements the session reference count by 1 and if the number reaches 0,
  // makes a call to the subsystem to stop notifications from this
  // characteristic.
  void RemoveNotifySession(const base::Closure& callback);

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class BluetoothRemoteGattServiceBlueZ;

  typedef std::pair<NotifySessionCallback, ErrorCallback>
      PendingStartNotifyCall;

  BluetoothRemoteGattCharacteristicBlueZ(
      BluetoothRemoteGattServiceBlueZ* service,
      const dbus::ObjectPath& object_path);
  ~BluetoothRemoteGattCharacteristicBlueZ() override;

  // bluez::BluetoothGattDescriptorClient::Observer overrides.
  void GattDescriptorAdded(const dbus::ObjectPath& object_path) override;
  void GattDescriptorRemoved(const dbus::ObjectPath& object_path) override;
  void GattDescriptorPropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) override;

  // Called by dbus:: on unsuccessful completion of a request to read or write
  // the characteristic value.
  void OnError(const ErrorCallback& error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // Called by dbus:: on successful completion of a request to start
  // notifications.
  void OnStartNotifySuccess(const NotifySessionCallback& callback);

  // Called by dbus:: on unsuccessful completion of a request to start
  // notifications.
  void OnStartNotifyError(const ErrorCallback& error_callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Called by dbus:: on successful completion of a request to stop
  // notifications.
  void OnStopNotifySuccess(const base::Closure& callback);

  // Called by dbus:: on unsuccessful completion of a request to stop
  // notifications.
  void OnStopNotifyError(const base::Closure& callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // Calls StartNotifySession for each queued request.
  void ProcessStartNotifyQueue();

  // Object path of the D-Bus characteristic object.
  dbus::ObjectPath object_path_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothRemoteGattServiceBlueZ* service_;

  // The total number of currently active value update sessions.
  size_t num_notify_sessions_;

  // Calls to StartNotifySession that are pending. This can happen during the
  // first remote call to start notifications.
  std::queue<PendingStartNotifyCall> pending_start_notify_calls_;

  // True, if a Start or Stop notify call to bluetoothd is currently pending.
  bool notify_call_pending_;

  // Mapping from GATT descriptor object paths to descriptor objects owned by
  // this characteristic. Since the Chrome OS implementation uses object paths
  // as unique identifiers, we also use this mapping to return descriptors by
  // identifier.
  typedef std::map<dbus::ObjectPath, BluetoothRemoteGattDescriptorBlueZ*>
      DescriptorMap;
  DescriptorMap descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicBlueZ>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_
