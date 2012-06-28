// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#pragma once

#include <map>
#include <string>
#include <vector>

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

class BluetoothAdapterClient;

// BluetoothDeviceClient is used to communicate with a bluetooth Device
// interface.
class CHROMEOS_EXPORT BluetoothDeviceClient {
 public:
  // Structure of properties associated with bluetooth devices.
  struct Properties : public BluetoothPropertySet {
    // The Bluetooth device address of the device. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth friendly name of the device. Read-only, to give a
    // different local name, use the |alias| property.
    dbus::Property<std::string> name;

    // Unique numeric identifier for the vendor of the device. Read-only.
    dbus::Property<uint16> vendor;

    // Unique vendor-assigned product identifier for the product of the
    // device. Read-only.
    dbus::Property<uint16> product;

    // Unique vendor-assigned version identifier for the device. Read-only.
    dbus::Property<uint16> version;

    // Proposed icon name for the device according to the freedesktop.org
    // icon naming specification. Read-only.
    dbus::Property<std::string> icon;

    // The Bluetooth class of the device. Read-only.
    dbus::Property<uint32> bluetooth_class;

    // List of 128-bit UUIDs that represent the available remote services.
    // Raed-only.
    dbus::Property<std::vector<std::string> > uuids;

    // List of characteristics-based available remote services. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > services;

    // Indicates that the device is currently paired. Read-only.
    dbus::Property<bool> paired;

    // Indicates that the device is currently connected. Read-only.
    dbus::Property<bool> connected;

    // Whether the device is trusted, and connections should be always
    // accepted and attempted when the device is visible.
    dbus::Property<bool> trusted;

    // Whether the device is blocked, connections will be always rejected
    // and the device will not be visible.
    dbus::Property<bool> blocked;

    // Local alias for the device, if not set, is equal to |name|.
    dbus::Property<std::string> alias;

    // List of object paths of nodes the device provides. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > nodes;

    // Object path of the adapter the device belongs to. Read-only.
    dbus::Property<dbus::ObjectPath> adapter;

    // Indicates whether the device is likely to only support pre-2.1
    // PIN Code pairing rather than 2.1 Secure Simple Pairing, this can
    // give false positives. Read-only.
    dbus::Property<bool> legacy_pairing;

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
    virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name) {}

    // Called when the device with object path |object_path| is about
    // to be disconnected, giving a chance for application layers to
    // shut down cleanly.
    virtual void DisconnectRequested(const dbus::ObjectPath& object_path) {}

    // Called when the device with object path |object_path| has a new
    // persistent device node with object path |node_path|.
    virtual void NodeCreated(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& node_path) {}

    // Called when the device with object path |object_path| removes
    // the persistent device node with object path |node_path|.
    virtual void NodeRemoved(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& node_path) {}
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

  // The Services map is used to convey the set of services discovered
  // on a device. The keys are unique record handles and the values are
  // XML-formatted service records. Both can be generated using the
  // sdptool(1) binary distributed with bluetoothd.
  typedef std::map<const uint32, std::string> ServiceMap;

  // The ServicesCallback is used for the DiscoverServices() method. It
  // receives three arguments, the |object_path| of the device, the
  // dictionary of the |services| discovered where the keys are unique
  // record handles and the values are XML formatted service records,
  // and |success| which indicates whether or not the request succeded.
  typedef base::Callback<void(const dbus::ObjectPath&, const ServiceMap&,
                              bool)> ServicesCallback;

  // Starts the service discovery process for the device with object path
  // |object_path|, the |pattern| paramter can be used to specify specific
  // UUIDs while an empty string will look for the public browse group.
  virtual void DiscoverServices(const dbus::ObjectPath& object_path,
                                const std::string& pattern,
                                const ServicesCallback& callback) = 0;

  // The DeviceCallback is used for device methods that only return to
  // indicate success. It receives two arguments, the |object_path| of the
  // device the call was made on and |success| which indicates whether or
  // not the request succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> DeviceCallback;

  // Cancels any previous service discovery processes for the device with
  // object path |object_path|.
  virtual void CancelDiscovery(const dbus::ObjectPath& object_path,
                               const DeviceCallback& callback) = 0;

  // Disconnects the device with object path |object_path|, terminating
  // the low-level ACL connection and any application connections using it.
  // Actual disconnection takes place after two seconds during which a
  // DisconnectRequested signal is emitted by the device to allow those
  // applications to terminate gracefully.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const DeviceCallback& callback) = 0;

  // The NodeCallback is used for device methods that return a dbus
  // object path for a persistent device node binding, as well as success.
  // It receives two arguments, the |object_path| of the persistent device
  // node binding object returned by the method and |success} which indicates
  // whether or not the request succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> NodeCallback;

  // Creates a persistent device node binding with the device with object path
  // |object_path| using the specified service |uuid|. The actual support
  // depends on the device driver, at the moment only RFCOMM TTY nodes are
  // supported.
  virtual void CreateNode(const dbus::ObjectPath& object_path,
                          const std::string& uuid,
                          const NodeCallback& callback) = 0;

  // Removes the persistent device node binding with the dbus object path
  // |node_path| from the device with object path |object_path|.
  virtual void RemoveNode(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& node_path,
                          const DeviceCallback& callback) = 0;

  // Creates the instance.
  static BluetoothDeviceClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus,
                                       BluetoothAdapterClient* adapter_client);

 protected:
  BluetoothDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
