// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_

#include <map>
#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace device {
class BluetoothUUID;
}

namespace content {

struct BluetoothScanFilter;

// Keeps track of which origins are allowed to access which devices and
// their services.
//
// |AddDevice| generates device ids, which are random strings that are unique
// for each (origin, device address) pair.
class CONTENT_EXPORT BluetoothAllowedDevicesMap final {
 public:
  BluetoothAllowedDevicesMap();
  ~BluetoothAllowedDevicesMap();

  // Adds the Bluetooth Device with |device_address| to the map of allowed
  // devices for that origin. Generates and returns a device id for the
  // (|origin|, |device_address|) pair.
  const std::string& AddDevice(
      const url::Origin& origin,
      const std::string& device_address,
      const std::vector<BluetoothScanFilter>& filters,
      const std::vector<device::BluetoothUUID>& optional_services);

  // Removes the Bluetooth Device with |device_address| from the map of allowed
  // devices for |origin|.
  void RemoveDevice(const url::Origin& origin,
                    const std::string& device_address);

  // TODO(ortuno): Add function to check if origin is allowed to access
  // a device's service and add tests for that function.
  // https://crbug.com/493460

  // Returns the Bluetooth Device's id for |origin|. Returns an empty string
  // if the origin is not allowed to access the device.
  const std::string& GetDeviceId(const url::Origin& origin,
                                 const std::string& device_address);

  // For |device_id| in |origin|, returns the Bluetooth device's address. If
  // there is no such |device_id| in |origin|, returns an empty string.
  const std::string& GetDeviceAddress(const url::Origin& origin,
                                      const std::string& device_id);

 private:
  typedef std::map<std::string, std::string> DeviceAddressToIdMap;
  typedef std::map<std::string, std::string> DeviceIdToAddressMap;
  typedef std::map<std::string, std::set<std::string>> DeviceIdToServicesMap;

  // Returns an id guaranteed to be unique for the origin. The id is randomly
  // generated so that an origin can't guess the id used in another origin.
  std::string GenerateDeviceId(const url::Origin& origin);
  std::set<std::string> UnionOfServices(
      const std::vector<BluetoothScanFilter>& filters,
      const std::vector<device::BluetoothUUID>& optional_services);

  std::map<url::Origin, DeviceAddressToIdMap>
      origin_to_device_address_to_id_map_;
  std::map<url::Origin, DeviceIdToAddressMap>
      origin_to_device_id_to_address_map_;
  std::map<url::Origin, DeviceIdToServicesMap>
      origin_to_device_id_to_services_map_;
};

}  //  namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ALLOWED_DEVICES_MAP_
