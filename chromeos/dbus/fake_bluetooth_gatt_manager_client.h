// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_manager_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class FakeBluetoothGattCharacteristicServiceProvider;
class FakeBluetoothGattDescriptorServiceProvider;
class FakeBluetoothGattServiceServiceProvider;

// FakeBluetoothGattManagerClient simulates the behavior of the Bluetooth
// daemon's GATT manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattManagerClient
    : public BluetoothGattManagerClient {
 public:
  FakeBluetoothGattManagerClient();
  virtual ~FakeBluetoothGattManagerClient();

  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE;

  // BluetoothGattManagerClient overrides.
  virtual void RegisterService(const dbus::ObjectPath& service_path,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) OVERRIDE;
  virtual void UnregisterService(const dbus::ObjectPath& service_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;

  // Register, unregister, and retrieve pointers to service, characteristic, and
  // descriptor service providers. Automatically called from the service
  // provider constructor and destructors.
  void RegisterServiceServiceProvider(
      FakeBluetoothGattServiceServiceProvider* provider);
  void RegisterCharacteristicServiceProvider(
      FakeBluetoothGattCharacteristicServiceProvider* provider);
  void RegisterDescriptorServiceProvider(
      FakeBluetoothGattDescriptorServiceProvider* provider);

  void UnregisterServiceServiceProvider(
      FakeBluetoothGattServiceServiceProvider* provider);
  void UnregisterCharacteristicServiceProvider(
      FakeBluetoothGattCharacteristicServiceProvider* provider);
  void UnregisterDescriptorServiceProvider(
      FakeBluetoothGattDescriptorServiceProvider* provider);

  // Return a pointer to the service provider that corresponds to the object
  // path |object_path| if it exists.
  FakeBluetoothGattServiceServiceProvider*
      GetServiceServiceProvider(const dbus::ObjectPath& object_path) const;
  FakeBluetoothGattCharacteristicServiceProvider*
      GetCharacteristicServiceProvider(
          const dbus::ObjectPath& object_path) const;
  FakeBluetoothGattDescriptorServiceProvider*
      GetDescriptorServiceProvider(const dbus::ObjectPath& object_path) const;

  // Returns true, if a GATT service with object path |object_path| was
  // registered with the GATT manager using RegisterService.
  bool IsServiceRegistered(const dbus::ObjectPath& object_path) const;

 private:
  // Mappings for GATT service, characteristic, and descriptor service
  // providers. The fake GATT manager stores references to all instances
  // created so that they can be obtained by tests.
  typedef std::map<
      dbus::ObjectPath, FakeBluetoothGattCharacteristicServiceProvider*>
      CharacteristicMap;
  typedef std::map<
      dbus::ObjectPath, FakeBluetoothGattDescriptorServiceProvider*>
      DescriptorMap;

  // The mapping for services is from object paths to pairs of boolean and
  // service provider pointer, where the boolean denotes whether or not the
  // service is already registered.
  typedef std::pair<bool, FakeBluetoothGattServiceServiceProvider*>
      ServiceProvider;
  typedef std::map<dbus::ObjectPath, ServiceProvider> ServiceMap;

  ServiceMap service_map_;
  CharacteristicMap characteristic_map_;
  DescriptorMap descriptor_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_
