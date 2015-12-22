// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothAdapterClient is used to communicate with objects representing
// local Bluetooth Adapters.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterClient : public BluezDBusClient {
 public:
  // A DiscoveryFilter represents a filter passed to the SetDiscoveryFilter
  // method.
  struct DiscoveryFilter {
    DiscoveryFilter();
    ~DiscoveryFilter();

    // Copy content of |filter| into this filter
    void CopyFrom(const DiscoveryFilter& filter);

    scoped_ptr<std::vector<std::string>> uuids;
    scoped_ptr<int16_t> rssi;
    scoped_ptr<uint16_t> pathloss;
    scoped_ptr<std::string> transport;

   private:
    DISALLOW_COPY_AND_ASSIGN(DiscoveryFilter);
  };

  // Structure of properties associated with bluetooth adapters.
  struct Properties : public dbus::PropertySet {
    // The Bluetooth device address of the adapter. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth system name, generally derived from the hostname.
    dbus::Property<std::string> name;

    // The Bluetooth friendly name of the adapter, unlike remote devices,
    // this property can be changed to change the presentation for when
    // the adapter is discoverable.
    dbus::Property<std::string> alias;

    // The Bluetooth class of the adapter device. Read-only.
    dbus::Property<uint32_t> bluetooth_class;

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
    dbus::Property<uint32_t> pairable_timeout;

    // The timeout in seconds to cease the adapter being discoverable by
    // other Bluetooth devices after |discoverable| is set to true. Zero
    // means adapter remains discoverable forever.
    dbus::Property<uint32_t> discoverable_timeout;

    // Indicates that the adapter is discovering other Bluetooth Devices.
    // Read-only. Use StartDiscovery() to begin discovery.
    dbus::Property<bool> discovering;

    // List of 128-bit UUIDs that represent the available local services.
    // Read-only.
    dbus::Property<std::vector<std::string>> uuids;

    // Local Device ID information in Linux kernel modalias format. Read-only.
    dbus::Property<std::string> modalias;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes from a local bluetooth adapter.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the adapter with object path |object_path| is added to the
    // system.
    virtual void AdapterAdded(const dbus::ObjectPath& object_path) {}

    // Called when the adapter with object path |object_path| is removed from
    // the system.
    virtual void AdapterRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}
  };

  ~BluetoothAdapterClient() override;

  // Adds and removes observers for events on all local bluetooth
  // adapters. Check the |object_path| parameter of observer methods to
  // determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of adapter object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetAdapters() = 0;

  // Obtain the properties for the adapter with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // The ErrorCallback is used by adapter methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  // Starts a device discovery on the adapter with object path |object_path|.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) = 0;

  // Cancels any previous device discovery on the adapter with object path
  // |object_path|.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

  // Removes from the adapter with object path |object_path| the remote
  // device with object path |object_path| from the list of known devices
  // and discards any pairing information.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) = 0;

  // Sets the device discovery filter on the adapter with object path
  // |object_path|. When this method is called with no filter parameter, filter
  // is removed.
  // SetDiscoveryFilter can be called before StartDiscovery. It is useful when
  // client will create first discovery session, to ensure that proper scan
  // will be started right after call to StartDiscovery.
  virtual void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                                  const DiscoveryFilter& discovery_filter,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static BluetoothAdapterClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnknownAdapterError[];

 protected:
  BluetoothAdapterClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterClient);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
