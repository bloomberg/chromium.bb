// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_INPUT_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_INPUT_CLIENT_H_
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

class BluetoothAdapterClient;

// BluetoothInputClient is used to communicate with the Input interface
// of a bluetooth device, rather than the generic device interface. Input
// devices are those conforming to the Bluetooth SIG HID (Human Interface
// Device) Profile such as keyboards, mice, trackpads and joysticks.
class CHROMEOS_EXPORT BluetoothInputClient {
 public:
  // Structure of properties associated with bluetooth input devices.
  struct Properties : public BluetoothPropertySet {
    // Indicates that the device is currently connected. Read-only.
    dbus::Property<bool> connected;

    Properties(dbus::ObjectProxy* object_proxy,
               PropertyChangedCallback callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a bluetooth input device.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the device with object path |object_path| has a
    // change in value of the input property named |property_name|.
    virtual void InputPropertyChanged(const dbus::ObjectPath& object_path,
                                      const std::string& property_name) {}
  };

  virtual ~BluetoothInputClient();

  // Adds and removes observers for events on all bluetooth input
  // devices. Check the |object_path| parameter of observer methods to
  // determine which device is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the input properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // The InputCallback is used for input device methods that only return to
  // indicate success. It receives two arguments, the |object_path| of the
  // input device the call was made on and |success| which indicates whether
  // or not the request succeeded.
  typedef base::Callback<void(const dbus::ObjectPath&, bool)> InputCallback;

  // The ConnectCallback is used for the Connect input device method to
  // indicate success. It receives a single argument, the |object_path| of
  // the input device the call was made on.
  typedef base::Callback<void(const dbus::ObjectPath&)> ConnectCallback;

  // The ConnectErrorCallback is used for the Connect input device method
  // to indicate failure. It receives three arguments, the |object_path| of
  // the input device the call was made on, the name of the error in
  // |error_name| and an optional message in |error_message|.
  typedef base::Callback<void(const dbus::ObjectPath& object_path,
                              const std::string& error_name,
                              const std::string& error_message)>
      ConnectErrorCallback;

  // Connects the input subsystem to the device with object path
  // |object_path|, which should already be a known device on the adapter.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const ConnectCallback& callback,
                       const ConnectErrorCallback& error_callback) = 0;

  // Disconnects the input subsystem from the device with object path
  // |object_path| without terminating the low-level ACL connection,
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const InputCallback& callback) = 0;

  // Creates the instance.
  static BluetoothInputClient* Create(DBusClientImplementationType type,
                                      dbus::Bus* bus,
                                      BluetoothAdapterClient* adapter_client);

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  BluetoothInputClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothInputClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_INPUT_CLIENT_H_
