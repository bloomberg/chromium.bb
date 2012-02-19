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
#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"
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
    // The Bluetooth device address of the adapter. Read-only.
    BluetoothProperty<std::string> address;

    // The Bluetooth friendly name of the adapter, unlike remote devices,
    // this property can be changed to change the presentation for when
    // the adapter is discoverable.
    BluetoothProperty<std::string> name;

    // The Bluetooth class of the adapter device. Read-only.
    BluetoothProperty<uint32> bluetooth_class;

    // Whether the adapter radio is powered.
    BluetoothProperty<bool> powered;

    // Whether the adapter is discoverable by other Bluetooth devices.
    // |discovering_timeout| is used to automatically disable after a time
    // period.
    BluetoothProperty<bool> discoverable;

    // Whether the adapter accepts incoming pairing requests from other
    // Bluetooth devices. |pairable_timeout| is used to automatically disable
    // after a time period.
    BluetoothProperty<bool> pairable;

    // The timeout in seconds to cease accepting incoming pairing requests
    // after |pairable| is set to true. Zero means adapter remains pairable
    // forever.
    BluetoothProperty<uint32> pairable_timeout;

    // The timeout in seconds to cease the adapter being discoverable by
    // other Bluetooth devices after |discoverable| is set to true. Zero
    // means adapter remains discoverable forever.
    BluetoothProperty<uint32> discoverable_timeout;

    // Indicates that the adapter is discovering other Bluetooth Devices.
    // Read-only. Use StartDiscovery() to begin discovery.
    BluetoothProperty<bool> discovering;

    // List of object paths of known Bluetooth devices, known devices are
    // those that have previously been connected or paired or are currently
    // connected or paired.
    BluetoothProperty<std::vector<dbus::ObjectPath> > devices;

    // List of 128-bit UUIDs that represent the available local services.
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

    // Called when the adapter with object path |object_path| discovers
    // a new remote device with address |address| and properties
    // |properties|, there is no device object path until connected.
    //
    // |properties| supports only value() calls, not Get() or Set(), and
    // should be copied if needed.
    virtual void DeviceFound(
        const dbus::ObjectPath& object_path, const std::string& address,
        const BluetoothDeviceClient::Properties& properties) {}

    // Called when the adapter with object path |object_path| can no
    // longer communicate with the discovered removed device with
    // address |address|.
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
