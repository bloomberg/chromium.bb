// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// BluetoothManagerClient is used to communicate with the bluetooth
// daemon's Manager interface.
class BluetoothManagerClient {
 public:
  // Interface for observing changes from the bluetooth manager.
  class Observer {
   public:
    virtual ~Observer() {}

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

  // The DefaultAdapterCallback receives two arguments:
  // dbus::ObjectPath object_path - the path of the new default adapter
  // bool success - whether or not the request succeeded
  typedef base::Callback<void(const dbus::ObjectPath&, bool)>
      DefaultAdapterCallback;

  // Retrieves the dbus object path for the default adapter.
  // The default adapter is the preferred local bluetooth interface when a
  // client does not specify a particular interface.
  virtual void DefaultAdapter(const DefaultAdapterCallback& callback) = 0;

  // Creates the instance.
  static BluetoothManagerClient* Create(dbus::Bus* bus);

 protected:
  BluetoothManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothManagerClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_MANAGER_CLIENT_H_
