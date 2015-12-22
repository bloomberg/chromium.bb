// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"

namespace device {

class BluetoothAdapter;
class BluetoothGattCharacteristic;

}  // namespace device

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothDeviceBlueZ;
class BluetoothRemoteGattCharacteristicBlueZ;
class BluetoothRemoteGattDescriptorBlueZ;

// The BluetoothRemoteGattServiceBlueZ class implements BluetootGattService
// for remote GATT services on the Chrome OS platform.
class BluetoothRemoteGattServiceBlueZ
    : public device::BluetoothGattService,
      public bluez::BluetoothGattServiceClient::Observer,
      public bluez::BluetoothGattCharacteristicClient::Observer {
 public:
  // device::BluetoothGattService overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsLocal() const override;
  bool IsPrimary() const override;
  device::BluetoothDevice* GetDevice() const override;
  std::vector<device::BluetoothGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<device::BluetoothGattService*> GetIncludedServices()
      const override;
  device::BluetoothGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;
  bool AddCharacteristic(
      device::BluetoothGattCharacteristic* characteristic) override;
  bool AddIncludedService(device::BluetoothGattService* service) override;
  void Register(const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void Unregister(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;

  // Object path of the underlying service.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Parses a named D-Bus error into a service error code.
  static device::BluetoothGattService::GattErrorCode DBusErrorToServiceError(
      const std::string error_name);

  // Returns the adapter associated with this service.
  BluetoothAdapterBlueZ* GetAdapter() const;

  // Notifies its observers that the GATT service has changed. This is mainly
  // used by BluetoothRemoteGattCharacteristicBlueZ instances to notify
  // service observers when characteristic descriptors get added and removed.
  void NotifyServiceChanged();

  // Notifies its observers that a descriptor |descriptor| belonging to
  // characteristic |characteristic| has been added or removed. This is used
  // by BluetoothRemoteGattCharacteristicBlueZ instances to notify service
  // observers when characteristic descriptors get added and removed. If |added|
  // is true, an "Added" event will be sent. Otherwise, a "Removed" event will
  // be sent.
  void NotifyDescriptorAddedOrRemoved(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      BluetoothRemoteGattDescriptorBlueZ* descriptor,
      bool added);

  // Notifies its observers that the value of a descriptor has changed. Called
  // by BluetoothRemoteGattCharacteristicBlueZ instances to notify service
  // observers.
  void NotifyDescriptorValueChanged(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      BluetoothRemoteGattDescriptorBlueZ* descriptor,
      const std::vector<uint8_t>& value);

 private:
  friend class BluetoothDeviceBlueZ;

  typedef std::map<dbus::ObjectPath, BluetoothRemoteGattCharacteristicBlueZ*>
      CharacteristicMap;

  BluetoothRemoteGattServiceBlueZ(BluetoothAdapterBlueZ* adapter,
                                  BluetoothDeviceBlueZ* device,
                                  const dbus::ObjectPath& object_path);
  ~BluetoothRemoteGattServiceBlueZ() override;

  // bluez::BluetoothGattServiceClient::Observer override.
  void GattServicePropertyChanged(const dbus::ObjectPath& object_path,
                                  const std::string& property_name) override;

  // bluez::BluetoothGattCharacteristicClient::Observer override.
  void GattCharacteristicAdded(const dbus::ObjectPath& object_path) override;
  void GattCharacteristicRemoved(const dbus::ObjectPath& object_path) override;
  void GattCharacteristicPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) override;

  // Object path of the GATT service.
  dbus::ObjectPath object_path_;

  // The adapter associated with this service. It's ok to store a raw pointer
  // here since |adapter_| indirectly owns this instance.
  BluetoothAdapterBlueZ* adapter_;

  // The device this GATT service belongs to. It's ok to store a raw pointer
  // here since |device_| owns this instance.
  BluetoothDeviceBlueZ* device_;

  // Mapping from GATT characteristic object paths to characteristic objects.
  // owned by this service. Since the Chrome OS implementation uses object
  // paths as unique identifiers, we also use this mapping to return
  // characteristics by identifier.
  CharacteristicMap characteristics_;

  // Indicates whether or not the characteristics of this service are known to
  // have been discovered.
  bool discovery_complete_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattServiceBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_
