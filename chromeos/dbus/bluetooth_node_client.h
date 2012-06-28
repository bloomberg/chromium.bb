// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_NODE_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_NODE_CLIENT_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class BluetoothDeviceClient;

// BluetoothNodeClient is used to represent persistent device nodes
// bound to bluetooth devices, such as RFCOMM TTY bindings to ttyX devices
// in Linux.
class CHROMEOS_EXPORT BluetoothNodeClient {
 public:
  // Structure of properties associated with persistent device nodes.
  struct Properties : public BluetoothPropertySet {
    // The name of the device node under /dev. Read-only.
    dbus::Property<std::string> name;

    // Object path of the device the node binding belongs to. Read-only.
    dbus::Property<dbus::ObjectPath> device;

    Properties(dbus::ObjectProxy* object_proxy,
               PropertyChangedCallback callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a persistent device node binding.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the node binding with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void NodePropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) {}
  };

  virtual ~BluetoothNodeClient();

  // Adds and removes observers for events on all persistent device node
  // bindings. Check the |object_path| parameter of observer methods to
  // determine which device node binding is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the node binding with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothNodeClient* Create(DBusClientImplementationType type,
                                     dbus::Bus* bus,
                                     BluetoothDeviceClient* device_client);

 protected:
  BluetoothNodeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothNodeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_NODE_CLIENT_H_
