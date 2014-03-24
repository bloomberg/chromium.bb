// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"

namespace chromeos {

// FakeBluetoothGattDescriptorClient simulates the behavior of the Bluetooth
// Daemon GATT characteristic descriptor objects and is used in test cases in
// place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothGattDescriptorClient
    : public BluetoothGattDescriptorClient {
 public:
  struct Properties : public BluetoothGattDescriptorClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothGattDescriptorClient();
  virtual ~FakeBluetoothGattDescriptorClient();

  // DBusClient override.
  virtual void Init(dbus::Bus* bus) OVERRIDE;

  // BluetoothGattDescriptorClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetDescriptors() OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;

 private:
  // TODO(armansito): Add simulated descriptors, once BlueZ's behavior is
  // better defined.

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattDescriptorClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
