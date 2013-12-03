// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothInputClient simulates the behavior of the Bluetooth Daemon
// input device objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothInputClient
    : public BluetoothInputClient {
 public:
  struct Properties : public BluetoothInputClient::Properties {
    explicit Properties(const PropertyChangedCallback & callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothInputClient();
  virtual ~FakeBluetoothInputClient();

  // BluetoothInputClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;

  // Simulate device addition/removal
  void AddInputDevice(const dbus::ObjectPath& object_path);
  void RemoveInputDevice(const dbus::ObjectPath& object_path);

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Static properties we return.
  typedef std::map<const dbus::ObjectPath, Properties *> PropertiesMap;
  PropertiesMap properties_map_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothInputClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_
