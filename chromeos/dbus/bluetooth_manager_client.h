// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// BluetoothManagerClient is used to communicate with the bluetooth
// daemon's Manager interface.
class CHROMEOS_EXPORT BluetoothManagerClient {
 public:
  // Structure of properties associated with the bluetooth manager.
  struct Properties : public BluetoothPropertySet {
    // List of object paths of local Bluetooth adapters. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > adapters;

    Properties(dbus::ObjectProxy* object_proxy,
               PropertyChangedCallback callback);
    virtual ~Properties();
  };

  // Interface for observing changes from the bluetooth manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the manager has a change in value of the property
    // named |property_name|.
    virtual void ManagerPropertyChanged(const std::string& property_name) {}

    // Called when a local bluetooth adapter is added.
    // |object_path| is the dbus object path of the adapter.
    virtual void AdapterAdded(const dbus::ObjectPath& object_path) {}

    // Called when a local bluetooth adapter is removed.
    // |object_path| is the dbus object path of the adapter.
    virtual void AdapterRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the default local bluetooth adapter changes.
    // |object_path| is the dbus object path of the new default adapter.
    // Not called if all adapters are removed.
    virtual void DefaultAdapterChanged(const dbus::ObjectPath& object_path) {}
  };

  virtual ~BluetoothManagerClient();

  // Adds and removes observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the manager, any values should be copied
  // if needed.
  virtual Properties* GetProperties() = 0;

  // The AdapterCallback is used for both the DefaultAdapter() and
  // FindAdapter() methods. It receives two arguments, the |object_path|
  // of the adapter and |success| which indicates whether or not the request
  // succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> AdapterCallback;

  // Retrieves the dbus object path for the default adapter.
  // The default adapter is the preferred local bluetooth interface when a
  // client does not specify a particular interface.
  virtual void DefaultAdapter(const AdapterCallback& callback) = 0;

  // Retrieves the dbus object path for the adapter with the address |address|,
  // which may also be an interface name.
  virtual void FindAdapter(const std::string& address,
                           const AdapterCallback& callback) = 0;

  // Creates the instance.
  static BluetoothManagerClient* Create(DBusClientImplementationType type,
                                        dbus::Bus* bus);

 protected:
  BluetoothManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
