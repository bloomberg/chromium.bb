// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dbus/bluetooth_property.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class BluetoothManagerClient;

// BluetoothAdapterClient is used to communicate with a bluetooth Adapter
// interface.
class BluetoothAdapterClient {
 public:
  // Structure of properties associated with bluetooth adapters.
  struct Properties : public BluetoothPropertySet {
    BluetoothProperty<std::string> address;
    BluetoothProperty<std::string> name;
    BluetoothProperty<uint32> bluetooth_class;
    BluetoothProperty<bool> powered;
    BluetoothProperty<bool> discoverable;
    BluetoothProperty<bool> pairable;
    BluetoothProperty<uint32> pairable_timeout;
    BluetoothProperty<uint32> discoverable_timeout;
    BluetoothProperty<bool> discovering;
    BluetoothProperty<std::vector<dbus::ObjectPath> > devices;
    BluetoothProperty<std::vector<std::string> > uuids;

    Properties(dbus::ObjectProxy* object_proxy,
               PropertyChangedCallback callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a local bluetooth adapter.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void PropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name) {}

    // Called when a new known device has been created.
    virtual void DeviceCreated(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& device_path) {}

    // Called when a previously known device is removed.
    virtual void DeviceRemoved(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& device_path) {}

    // Called when a new remote device has been discovered.
    // |device_properties| should be copied if needed.
    virtual void DeviceFound(const dbus::ObjectPath& object_path,
                             const std::string& address,
                             const DictionaryValue& device_properties) {}

    // Called when a previously discovered device is no longer visible.
    virtual void DeviceDisappeared(const dbus::ObjectPath& object_path,
                                   const std::string& address) {}
  };

  virtual ~BluetoothAdapterClient();

  // Adds and removes observers for events on the adapter with object path
  // |object_path|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the adapter with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Starts a device discovery on the adapter with object path |object_path|.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path) = 0;

  // Cancels any previous device discovery on the adapter with object path
  // |object_path|.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothAdapterClient* Create(dbus::Bus* bus,
                                        BluetoothManagerClient* manager_client);

 protected:
  BluetoothAdapterClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
