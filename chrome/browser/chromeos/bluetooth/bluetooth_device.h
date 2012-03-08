// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class BluetoothAdapter;

// The BluetoothDevice class represents a remote Bluetooth device, both
// its properties and capabilities as discovered by a local adapter and
// actions that may be performed on the remove device such as pairing,
// connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the address() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class BluetoothDevice : public BluetoothDeviceClient::Observer {
 public:
  // Possible values that may be returned by GetDeviceType(), representing
  // different types of bluetooth device that we support or are aware of
  // decoded from the bluetooth class information.
  enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_COMPUTER,
    DEVICE_PHONE,
    DEVICE_MODEM,
    DEVICE_PERIPHERAL,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_TABLET,
    DEVICE_KEYBOARD_MOUSE_COMBO
  };

  // Interface for observing changes from bluetooth devices.
  class Observer {
   public:
    virtual ~Observer() {}

    // TODO(keybuk): add observers for pairing and connection.
  };

  virtual ~BluetoothDevice();

  // Returns the Bluetooth of address the device. This should be used as
  // a unique key to identify the device and copied where needed.
  const std::string& address() const { return address_; }

  // Returns the name of the device suitable for displaying, this may
  // be a synthesied string containing the address and localized type name
  // if the device has no obtained name.
  string16 GetName() const;

  // Returns the type of the device, limited to those we support or are
  // aware of, by decoding the bluetooth class information. The returned
  // values are unique, and do not overlap, so DEVICE_KEYBOARD is not also
  // DEVICE_PERIPHERAL.
  DeviceType GetDeviceType() const;

  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  string16 GetAddressWithLocalizedDeviceTypeName() const;

  // Indicates whether the class of this device is supported by Chrome OS.
  bool IsSupported() const;

  // Indicates if the device was one discovered by the adapter, and not yet
  // paired with or connected to.
  bool WasDiscovered() const;

  // Indicates whether the device is paired to the adapter, note that some
  // devices can be connected to and used without pairing so use
  // WasDiscovered() rather than this method to determine whether or not
  // to display in a list of devices.
  bool IsPaired() const { return paired_; }

  // Indicates whether the device is currently connected to the adapter
  // and at least one service available for use.
  bool IsConnected() const;

 private:
  friend class BluetoothAdapter;

  BluetoothDevice();

  // Sets the dbus object path for the device to |object_path|, indicating
  // that the device has gone from being discovered to paired or connected.
  void SetObjectPath(const dbus::ObjectPath& object_path);

  // Updates device information from the properties in |properties|, device
  // state properties such as |paired_| and |connected_| are ignored unless
  // |update_state| is true.
  void Update(const BluetoothDeviceClient::Properties* properties,
              bool update_state);

  // Creates a new BluetoothDevice object bound to the information of the
  // dbus object path |object_path|, representing a paired device or one
  // that is currently or previously connected, with initial properties set
  // from |properties|.
  static BluetoothDevice* CreateBound(
      const dbus::ObjectPath& object_path,
      const BluetoothDeviceClient::Properties* properties);

  // Creates a new BluetoothDevice object not bound to a dbus object path,
  // representing a discovered device that has not yet been connected to,
  // with initial properties set from |properties|.
  static BluetoothDevice* CreateUnbound(
      const BluetoothDeviceClient::Properties* properties);

  // The dbus object path of the device, will be empty if the device has only
  // been discovered and not yet paired with or connected to.
  dbus::ObjectPath object_path_;

  // The Bluetooth address of the device.
  std::string address_;

  // The name of the device, as supplied by the remote device.
  std::string name_;

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class_;

  // Tracked device state, updated by the adapter managing the lifecyle of
  // the device.
  bool paired_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDevice);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
