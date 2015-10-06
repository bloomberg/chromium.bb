// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"

namespace chromeos {

// BluetoothGattCharacteristicServiceProvider is used to provide a D-Bus object
// that represents a local GATT characteristic that the Bluetooth daemon can
// communicate with.
//
// Instantiate with a chosen D-Bus object path, delegate, and other fields.
// The Bluetooth daemon communicates with a GATT characteristic using the
// standard DBus.Properties interface. While most properties of the GATT
// characteristic interface are read-only and don't change throughout the
// life-time of the object, the "Value" property is both writeable and its
// value can change. Both Get and Set operations performed on the "Value"
// property are delegated to the Delegate object, an instance of which is
// mandatory during initialization. In addition, a "SendValueChanged" method is
// provided, which emits a DBus.Properties.PropertyChanged signal for the
// "Value" property.
class CHROMEOS_EXPORT BluetoothGattCharacteristicServiceProvider {
 public:
  // Interface for reacting to GATT characteristic value requests.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // ValueCallback is used for methods that require a characteristic value
    // to be returned.
    typedef base::Callback<void(const std::vector<uint8>&)> ValueCallback;

    // ErrorCallback is used by methods to report failure.
    typedef base::Closure ErrorCallback;

    // This method will be called when a remote device requests to read the
    // value of the exported GATT characteristic. Invoke |callback| with a value
    // to return that value to the requester. Invoke |error_callback| to report
    // a failure to read the value. This can happen, for example, if the
    // characteristic has no read permission set. Either callback should be
    // invoked after a reasonable amount of time, since the request will time
    // out if left pending for too long.
    virtual void GetCharacteristicValue(
        const ValueCallback& callback,
        const ErrorCallback& error_callback) = 0;

    // This method will be called, when a remote device requests to write the
    // value of the exported GATT characteristic. Invoke |callback| to report
    // that the value was successfully written. Invoke |error_callback| to
    // report a failure to write the value. This can happen, for example, if the
    // characteristic has no write permission set. Either callback should be
    // invoked after a reasonable amount of time, since the request will time
    // out if left pending for too long.
    //
    // The delegate should use this method to perform any side-effects that may
    // occur based on the set value and potentially send a property changed
    // signal to notify the Bluetooth daemon that the value has changed.
    virtual void SetCharacteristicValue(
        const std::vector<uint8>& value,
        const base::Closure& callback,
        const ErrorCallback& error_callback) = 0;
  };

  virtual ~BluetoothGattCharacteristicServiceProvider();

  // Send a PropertyChanged signal to notify the Bluetooth daemon that the value
  // of the "Value" property has changed to |value|.
  virtual void SendValueChanged(const std::vector<uint8>& value) = 0;

  // Creates the instance, where |bus| is the D-Bus bus connection to export
  // the object onto, |uuid| is the 128-bit GATT characteristic UUID,
  // |flags| is the list of GATT characteristic properties, |permissions| is the
  // list of attribute permissions, |service_path| is the object path of the
  // exported GATT service the characteristic belongs to, |object_path| is the
  // object path that the characteristic should have, and |delegate| is the
  // object that "Value" Get/Set requests will be passed to and responses
  // generated from.
  //
  // Object paths of GATT characteristics must be hierarchical to the path of
  // the GATT service they belong to. Hence, |object_path| must have
  // |service_path| as its prefix. Ownership of |delegate| is not taken, thus
  // the delegate should outlive this instance. A delegate should handle only
  // a single exported characteristic and own it.
  static BluetoothGattCharacteristicServiceProvider* Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const std::vector<std::string>& permissions,
      const dbus::ObjectPath& service_path);

 protected:
  BluetoothGattCharacteristicServiceProvider();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattCharacteristicServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
