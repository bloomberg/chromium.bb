// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace device {
struct BluetoothOutOfBandPairingData;
}  // namespace device

namespace chromeos {

// BluetoothOutOfBandClient is used to manage Out Of Band Pairing
// Data for the local adapter and remote devices.
class CHROMEOS_EXPORT BluetoothOutOfBandClient {
 public:
  virtual ~BluetoothOutOfBandClient();

  typedef base::Callback<void(
      const device::BluetoothOutOfBandPairingData& data,
      bool success)> DataCallback;

  typedef base::Callback<void(bool success)> SuccessCallback;

  // Reads the local Out Of Band Pairing Data and return it in |callback|.
  virtual void ReadLocalData(
      const dbus::ObjectPath& object_path,
      const DataCallback& callback) = 0;

  // Sets the Out Of Band Pairing Data for the device at |address| to |data|,
  // indicating success via |callback|.  Makes a copy of |data|.
  virtual void AddRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const device::BluetoothOutOfBandPairingData& data,
      const SuccessCallback& callback) = 0;

  // Clears the Out Of Band Pairing Data for the device at |address|, indicating
  // success via |callback|.
  virtual void RemoveRemoteData(
      const dbus::ObjectPath& object_path,
      const std::string& address,
      const SuccessCallback& callback) = 0;

  // Creates the instance.
  static BluetoothOutOfBandClient* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus);

 protected:
  BluetoothOutOfBandClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothOutOfBandClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_OUT_OF_BAND_CLIENT_H_
