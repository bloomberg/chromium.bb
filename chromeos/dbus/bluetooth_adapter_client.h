// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_property.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class BluetoothManagerClient;

// BluetoothAdapterClient is used to communicate with a bluetooth Adapter
// interface.
class CHROMEOS_EXPORT BluetoothAdapterClient {
 public:
  // Structure of properties associated with bluetooth adapters.
  struct Properties : public BluetoothPropertySet {
    // The Bluetooth device address of the adapter. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth friendly name of the adapter, unlike remote devices,
    // this property can be changed to change the presentation for when
    // the adapter is discoverable.
    dbus::Property<std::string> name;

    // The Bluetooth class of the adapter device. Read-only.
    dbus::Property<uint32> bluetooth_class;

    // Whether the adapter radio is powered.
    dbus::Property<bool> powered;

    // Whether the adapter is discoverable by other Bluetooth devices.
    // |discovering_timeout| is used to automatically disable after a time
    // period.
    dbus::Property<bool> discoverable;

    // Whether the adapter accepts incoming pairing requests from other
    // Bluetooth devices. |pairable_timeout| is used to automatically disable
    // after a time period.
    dbus::Property<bool> pairable;

    // The timeout in seconds to cease accepting incoming pairing requests
    // after |pairable| is set to true. Zero means adapter remains pairable
    // forever.
    dbus::Property<uint32> pairable_timeout;

    // The timeout in seconds to cease the adapter being discoverable by
    // other Bluetooth devices after |discoverable| is set to true. Zero
    // means adapter remains discoverable forever.
    dbus::Property<uint32> discoverable_timeout;

    // Indicates that the adapter is discovering other Bluetooth Devices.
    // Read-only. Use StartDiscovery() to begin discovery.
    dbus::Property<bool> discovering;

    // List of object paths of known Bluetooth devices, known devices are
    // those that have previously been connected or paired or are currently
    // connected or paired. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > devices;

    // List of 128-bit UUIDs that represent the available local services.
    // Read-only.
    dbus::Property<std::vector<std::string> > uuids;

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
    virtual void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}

    // Called when the adapter with object path |object_path| has a
    // new known device with object path |object_path|.
    virtual void DeviceCreated(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& device_path) {}

    // Called when the adapter with object path |object_path| removes
    // the known device with object path |object_path|.
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

  // Adds and removes observers for events on all local bluetooth
  // adapters. Check the |object_path| parameter of observer methods to
  // determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the adapter with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // The AdapterCallback is used for adapter methods that only return to
  // indicate success. It receives two arguments, the |object_path| of the
  // adapter the call was made on and |success| which indicates whether
  // or not the request succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> AdapterCallback;

  // Request a client session for the adapter with object path |object_path|,
  // possible mode changes must be confirmed by the user via a registered
  // agent.
  virtual void RequestSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) = 0;

  // Release a previously requested session, restoring the adapter mode to
  // that prior to the original request.
  virtual void ReleaseSession(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) = 0;

  // Starts a device discovery on the adapter with object path |object_path|.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const AdapterCallback& callback) = 0;

  // Cancels any previous device discovery on the adapter with object path
  // |object_path|.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const AdapterCallback& callback) = 0;

  // The DeviceCallback is used for adapter methods that return a dbus
  // object path for a remote device, as well as success. It receives two
  // arguments, the |object_path| of the device returned by the method and
  // |success| which indicates whether or not the request succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> DeviceCallback;

  // Retrieves the dbus object path from the adapter with object path
  // |object_path| for the known device with the address |address|.
  virtual void FindDevice(const dbus::ObjectPath& object_path,
                          const std::string& address,
                          const DeviceCallback& callback) = 0;

  // The CreateDeviceCallback is used for the CreateDevice and
  // CreatePairedDevice adapter methods that return a dbus object path for
  // a remote device on success. It receives a single argument, the
  // |object_path| of the device returned by the method.
  typedef base::Callback<void(const dbus::ObjectPath&)> CreateDeviceCallback;

  // The CreateDeviceErrorCallback is used for the CreateDevice and
  // CreatePairedDevices adapter methods to indicate failure. It receives
  // two arguments, the name of the error in |error_name| and an optional
  // message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)>
      CreateDeviceErrorCallback;

  // Creates a new dbus object from the adapter with object path |object_path|
  // to the remote device with address |address|, connecting to it and
  // retrieving all SDP records. After a successful call, the device is known
  // and appear's in the adapter's |devices| interface. This is a low-security
  // connection which may not be accepted by the device.
  virtual void CreateDevice(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) = 0;

  // Creates a new dbus object from the adapter with object path |object_path|
  // to the remote device with address |address|, connecting to it, retrieving
  // all SDP records and then initiating a pairing. If CreateDevice() has been
  // previously called for this device, this only initiates the pairing.
  //
  // The dbus object path |agent_path| of an agent within the local process
  // must be specified to negotiate the pairing, |capability| specifies the
  // input and display capabilities of that agent and should be one of the
  // constants declared in the bluetooth_agent:: namespace.
  virtual void CreatePairedDevice(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const dbus::ObjectPath& agent_path,
      const std::string& capability,
      const CreateDeviceCallback& callback,
      const CreateDeviceErrorCallback& error_callback) = 0;

  // Cancels the currently in progress call to CreateDevice() or
  // CreatePairedDevice() on the adapter with object path |object_path|
  // for the remote device with address |address|.
  virtual void CancelDeviceCreation(const dbus::ObjectPath& object_path,
                                    const std::string& address,
                                    const AdapterCallback& callback) = 0;

  // Removes from the adapter with object path |object_path| the remote
  // device with object path |object_path| from the list of known devices
  // and discards any pairing information.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const AdapterCallback& callback) = 0;

  // Registers an adapter-wide agent for the adapter with object path
  // |object_path|. This agent is used for incoming pairing connections
  // and confirmation of adapter mode changes. The dbus object path
  // |agent_path| of an agent within the local process must be specified,
  // |capability| specifies the input and display capabilities of that
  // agent and should be one of the constants declared in the
  // bluetooth_agent:: namespace.
  virtual void RegisterAgent(const dbus::ObjectPath& object_path,
                             const dbus::ObjectPath& agent_path,
                             const std::string& capability,
                             const AdapterCallback& callback) = 0;

  // Unregisters an adapter-wide agent with object path |agent_path| from
  // the adapter with object path |object_path|.
  virtual void UnregisterAgent(const dbus::ObjectPath& object_path,
                               const dbus::ObjectPath& agent_path,
                               const AdapterCallback& callback) = 0;

  // Creates the instance.
  static BluetoothAdapterClient* Create(DBusClientImplementationType type,
                                        dbus::Bus* bus,
                                        BluetoothManagerClient* manager_client);

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kBadResponseError[];

 protected:
  BluetoothAdapterClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
