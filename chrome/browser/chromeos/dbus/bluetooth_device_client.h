// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
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

class BluetoothAdapterClient;

// BluetoothDeviceClient is used to communicate with a bluetooth Device
// interface.
class BluetoothDeviceClient {
 public:
  // Structure of properties associated with bluetooth devices.
  struct Properties : public BluetoothPropertySet {
    // The Bluetooth device address of the device. Read-only.
    BluetoothProperty<std::string> address;

    // The Bluetooth friendly name of the device. Read-only, to give a
    // different local name, use the |alias| property.
    BluetoothProperty<std::string> name;

    // Unique numeric identifier for the vendor of the device. Read-only.
    BluetoothProperty<uint16> vendor;

    // Unique vendor-assigned product identifier for the product of the
    // device. Read-only.
    BluetoothProperty<uint16> product;

    // Unique vendor-assigned version identifier for the device. Read-only.
    BluetoothProperty<uint16> version;

    // Proposed icon name for the device according to the freedesktop.org
    // icon naming specification. Read-only.
    BluetoothProperty<std::string> icon;

    // The Bluetooth class of the device. Read-only.
    BluetoothProperty<uint32> bluetooth_class;

    // List of 128-bit UUIDs that represent the available remote services.
    // Raed-only.
    BluetoothProperty<std::vector<std::string> > uuids;

    // List of characteristics-based available remote services. Read-only.
    BluetoothProperty<std::vector<dbus::ObjectPath> > services;

    // Indicates that the device is currently paired. Read-only.
    BluetoothProperty<bool> paired;

    // Indicates that the device is currently connected. Read-only.
    BluetoothProperty<bool> connected;

    // Whether the device is trusted, and connections should be always
    // accepted and attempted when the device is visible.
    BluetoothProperty<bool> trusted;

    // Whether the device is blocked, connections will be always rejected
    // and the device will not be visible.
    BluetoothProperty<bool> blocked;

    // Local alias for the device, if not set, is equal to |name|.
    BluetoothProperty<std::string> alias;

    // List of object paths of nodes the device provides. Read-only.
    BluetoothProperty<std::vector<dbus::ObjectPath> > nodes;

    // Object path of the adapter the device belongs to. Read-only.
    BluetoothProperty<dbus::ObjectPath> adapter;

    // Indicates whether the device is likely to only support pre-2.1
    // PIN Code pairing rather than 2.1 Secure Simple Pairing, this can
    // give false positives. Read-only.
    BluetoothProperty<bool> legacy_pairing;

    Properties(dbus::ObjectProxy* object_proxy,
               PropertyChangedCallback callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a remote bluetooth device.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the device with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void PropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name) {}
  };

  virtual ~BluetoothDeviceClient();

  // Adds and removes observers for events on all remote bluetooth
  // devices. Check the |object_path| parameter of observer methods to
  // determine which device is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothDeviceClient* Create(dbus::Bus* bus,
                                       BluetoothAdapterClient* adapter_client);

 protected:
  BluetoothDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
