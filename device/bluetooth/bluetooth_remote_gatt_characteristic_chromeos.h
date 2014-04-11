// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;

}  // namespace device

namespace chromeos {

class BluetoothRemoteGattDescriptorChromeOS;
class BluetoothRemoteGattServiceChromeOS;

// The BluetoothRemoteGattCharacteristicChromeOS class implements
// BluetoothGattCharacteristic for remote GATT characteristics on the Chrome OS
// platform.
class BluetoothRemoteGattCharacteristicChromeOS
    : public device::BluetoothGattCharacteristic,
      public BluetoothGattDescriptorClient::Observer {
 public:
  // device::BluetoothGattCharacteristic overrides.
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual device::BluetoothUUID GetUUID() const OVERRIDE;
  virtual bool IsLocal() const OVERRIDE;
  virtual const std::vector<uint8>& GetValue() const OVERRIDE;
  virtual device::BluetoothGattService* GetService() const OVERRIDE;
  virtual Properties GetProperties() const OVERRIDE;
  virtual Permissions GetPermissions() const OVERRIDE;
  virtual std::vector<device::BluetoothGattDescriptor*>
      GetDescriptors() const OVERRIDE;
  virtual bool AddDescriptor(
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual bool UpdateValue(const std::vector<uint8>& value) OVERRIDE;
  virtual void ReadRemoteCharacteristic(
      const ValueCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void WriteRemoteCharacteristic(
      const std::vector<uint8>& new_value,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class BluetoothRemoteGattServiceChromeOS;

  BluetoothRemoteGattCharacteristicChromeOS(
      BluetoothRemoteGattServiceChromeOS* service,
      const dbus::ObjectPath& object_path);
  virtual ~BluetoothRemoteGattCharacteristicChromeOS();

  // BluetoothGattDescriptorClient::Observer overrides.
  virtual void GattDescriptorAdded(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattDescriptorRemoved(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattDescriptorPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // Called by dbus:: on completion of the request to get the characteristic
  // value.
  void OnGetValue(const ValueCallback& callback,
                  const ErrorCallback& error_callback,
                  bool success);

  // Called by dbus:: on completion of the request to set the characteristic
  // value.
  void OnSetValue(const base::Closure& callback,
                  const ErrorCallback& error_callback,
                  bool success);

  // Object path of the D-Bus characteristic object.
  dbus::ObjectPath object_path_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothRemoteGattServiceChromeOS* service_;

  // Mapping from GATT descriptor object paths to descriptor objects owned by
  // this characteristic. Since the Chrome OS implementation uses object paths
  // as unique identifiers, we also use this mapping to return descriptors by
  // identifier.
  typedef std::map<dbus::ObjectPath, BluetoothRemoteGattDescriptorChromeOS*>
      DescriptorMap;
  DescriptorMap descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicChromeOS>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_
