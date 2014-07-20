// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"

namespace device {
namespace win {

// Represents a device registry property value
class DeviceRegistryPropertyValue {
 public:
  // Creates a property value instance, where |property_type| is one of REG_xxx
  // registry value type (e.g. REG_SZ, REG_DWORD), |value| is a byte array
  // containing the propery value and |value_size| is the number of bytes in
  // |value|. Note the returned instance takes ownership of the bytes in
  // |value|.
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

// Represents the value associated to a DEVPROPKEY.
class DevicePropertyValue {
 public:
  // Creates a property value instance, where |property_type| is one of
  // DEVPROP_TYPE_xxx value type , |value| is a byte array containing the
  // propery value and |value_size| is the number of bytes in |value|. Note the
  // returned instance takes ownership of the bytes in |value|.
  DevicePropertyValue(DEVPROPTYPE property_type,
                      scoped_ptr<uint8_t[]> value,
                      size_t value_size);

  DEVPROPTYPE property_type() const { return property_type_; }

  uint32_t AsUint32() const;

 private:
  DEVPROPTYPE property_type_;
  scoped_ptr<uint8_t[]> value_;
  size_t value_size_;

  DISALLOW_COPY_AND_ASSIGN(DevicePropertyValue);
};

// Returns true only on Windows platforms supporting Bluetooth Low Energy.
bool IsBluetoothLowEnergySupported();

struct BluetoothLowEnergyServiceInfo {
  BluetoothLowEnergyServiceInfo();
  ~BluetoothLowEnergyServiceInfo();

  BTH_LE_UUID uuid;
};

struct BluetoothLowEnergyDeviceInfo {
  BluetoothLowEnergyDeviceInfo();
  ~BluetoothLowEnergyDeviceInfo();

  base::FilePath path;
  std::string id;
  std::string friendly_name;
  BLUETOOTH_ADDRESS address;
  bool visible;
  bool authenticated;
  bool connected;
};

// Enumerates the list of known (i.e. already paired) Bluetooth LE devices on
// this machine. In case of error, returns false and sets |error| with an error
// message describing the problem.
// Note: This function returns an error if Bluetooth Low Energy is not supported
// on this Windows platform.
bool EnumerateKnownBluetoothLowEnergyDevices(
    ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
    std::string* error);

// Enumerates the list of known (i.e. cached) GATT services for a given
// Bluetooth LE device |device_info| into |services|. In case of error, returns
// false and sets |error| with an error message describing the problem. Note:
// This function returns an error if Bluetooth Low Energy is not supported on
// this Windows platform.
bool EnumerateKnownBluetoothLowEnergyServices(
    BluetoothLowEnergyDeviceInfo* device_info,
    ScopedVector<BluetoothLowEnergyServiceInfo>* services,
    std::string* error);

bool ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
    const std::string& instance_id,
    BLUETOOTH_ADDRESS* btha,
    std::string* error);

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
