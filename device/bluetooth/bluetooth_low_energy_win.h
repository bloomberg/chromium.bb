// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_

#include <windows.h>
#include <setupapi.h>
// #include <bthledef.h>
// TODO(rpaquay):
// bthledef.h from Win8 SDK has a couple of issues when used in a Win32 app:
// * line 420: usage of "pragma pop" instead of "pragma warning(pop)"
// * line 349: no CALLBACK modifier in the definition of
// PFNBLUETOOTH_GATT_EVENT_CALLBACK.
//
// So, we duplicate the definitions we need and prevent the build from including
// the content of bthledef.h.
#ifndef __BTHLEDEF_H__
#define __BTHLEDEF_H__

//
// Bluetooth LE device interface GUID
//
// {781aee18-7733-4ce4-adb0-91f41c67b592}
DEFINE_GUID(GUID_BLUETOOTHLE_DEVICE_INTERFACE,
            0x781aee18,
            0x7733,
            0x4ce4,
            0xad,
            0xd0,
            0x91,
            0xf4,
            0x1c,
            0x67,
            0xb5,
            0x92);

#endif  // __BTHLEDEF_H__
#include <bluetoothapis.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/win/scoped_handle.h"

namespace device {
namespace win {

// Represents a device registry property value
class DeviceRegistryPropertyValue {
 public:
  // Creates a property value instance, where |property_type| is one of REG_xxx
  // registry value type (e.g. REG_SZ, REG_DWORD), |value| is a byte array
  // containing the propery value and |value_size| is the number of bytes in
  // |value|. Note the returned instance takes ownership of |value| array.
  static scoped_ptr<DeviceRegistryPropertyValue> Create(
      DWORD property_type,
      scoped_ptr<uint8_t[]> value,
      size_t value_size);
  ~DeviceRegistryPropertyValue();

  // Returns the vaue type a REG_xxx value (e.g. REG_SZ, REG_DWORD, ...)
  DWORD property_type() const { return property_type_; }

  std::string AsString() const;
  DWORD AsDWORD() const;

 private:
  DeviceRegistryPropertyValue(DWORD property_type,
                              scoped_ptr<uint8_t[]> value,
                              size_t value_size);

  DWORD property_type_;
  scoped_ptr<uint8_t[]> value_;
  size_t value_size_;

  DISALLOW_COPY_AND_ASSIGN(DeviceRegistryPropertyValue);
};

// Returns true only on Windows platforms supporting Bluetooth Low Energy.
bool IsBluetoothLowEnergySupported();

struct BluetoothLowEnergyDeviceInfo {
  BluetoothLowEnergyDeviceInfo() { address.ullLong = BLUETOOTH_NULL_ADDRESS; }

  base::FilePath path;
  std::string id;
  std::string friendly_name;
  BLUETOOTH_ADDRESS address;
};

// Enumerates the list of known (i.e. already paired) Bluetooth LE devices on
// this machine. In case of error, returns false and sets |error| with an error
// message describing the problem.
// Note: This function returns an error if Bluetooth Low Energy is not supported
// on this Windows platform.
bool EnumerateKnownBluetoothLowEnergyDevices(
    ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
    std::string* error);

bool ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
    const std::string& instance_id,
    BLUETOOTH_ADDRESS* btha,
    std::string* error);

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
