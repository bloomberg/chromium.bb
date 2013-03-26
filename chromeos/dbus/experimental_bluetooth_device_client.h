// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_DEVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// ExperimentalBluetoothDeviceClient is used to communicate with Bluetooth
// Device objects.
class CHROMEOS_EXPORT ExperimentalBluetoothDeviceClient {
 public:
  // Structure of properties associated with bluetooth devices.
  struct Properties : public dbus::PropertySet {
    // The Bluetooth device address of the device. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth friendly name of the device. Read-only, to give a
    // different local name, use the |alias| property.
    dbus::Property<std::string> name;

    // Proposed icon name for the device according to the freedesktop.org
    // icon naming specification. Read-only.
    dbus::Property<std::string> icon;

    // The Bluetooth class of the device. Read-only.
    dbus::Property<uint32> bluetooth_class;

    // The GAP external appearance of the device. Read-only.
    dbus::Property<uint16> appearance;

    // Unique numeric identifier for the vendor of the device. Read-only.
    dbus::Property<uint16> vendor;

    // List of 128-bit UUIDs that represent the available remote services.
    // Read-only.
    dbus::Property<std::vector<std::string> > uuids;

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

    // Object path of the adapter the device belongs to. Read-only.
    dbus::Property<dbus::ObjectPath> adapter;

    // Indicates whether the device is likely to only support pre-2.1
    // PIN Code pairing rather than 2.1 Secure Simple Pairing, this can
    // give false positives. Read-only.
    dbus::Property<bool> legacy_pairing;

    // Remote Device ID information in Linux kernel modalias format. Read-only.
    dbus::Property<std::string> modalias;

    // Received signal strength indicator. Read-only.
    dbus::Property<int16> rssi;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a remote bluetooth device.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the remote device with object path |object_path| is added
    // to the set of known devices.
    virtual void DeviceAdded(const dbus::ObjectPath& object_path) {}

    // Called when the remote device with object path |object_path| is removed
    // from the set of known devices.
    virtual void DeviceRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the device with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name) {}
  };

  virtual ~ExperimentalBluetoothDeviceClient();

  // Adds and removes observers for events on all remote bluetooth
  // devices. Check the |object_path| parameter of observer methods to
  // determine which device is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of device object paths associated with the given adapter
  // identified by the D-Bus object path |adapter_path|.
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) = 0;

  // Obtain the properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // The ErrorCallback is used by device methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  // Connects to the device with object path |object_path|, connecting any
  // profiles that can be connected to and have been flagged as auto-connected;
  // may be used to connect additional profiles for an already connected device,
  // and succeeds if at least one profile is connected.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  // Disconnects the device with object path |object_path|, terminating
  // the low-level ACL connection and any profiles using it.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Connects to the profile |uuid| on the device with object path
  // |object_path|, provided that the profile has been registered with a
  // handler on the local device.
  virtual void ConnectProfile(const dbus::ObjectPath& object_path,
                              const std::string& uuid,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) = 0;

  // Disconnects from the profile |uuid| on the device with object path
  // |object_path|.
  virtual void DisconnectProfile(const dbus::ObjectPath& object_path,
                                 const std::string& uuid,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Initiates pairing with the device with object path |object_path| and
  // retrieves all SDP records or GATT primary services. An agent must be
  // registered to handle the pairing request.
  virtual void Pair(const dbus::ObjectPath& object_path,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback) = 0;

  // Cancels an in-progress pairing with the device with object path
  // |object_path| initiated by Pair().
  virtual void CancelPairing(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static ExperimentalBluetoothDeviceClient* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus);

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnknownDeviceError[];

 protected:
  ExperimentalBluetoothDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_DEVICE_CLIENT_H_
