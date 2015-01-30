// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/bluetooth/bluetooth_device.h"

#include "base/strings/string_util.h"

namespace content {

BluetoothDevice::BluetoothDevice()
    : instance_id(""),
      name(base::string16()),
      device_class(0),
      vendor_id_source(
          device::BluetoothDevice::VendorIDSource::VENDOR_ID_UNKNOWN),
      vendor_id(0),
      product_id(0),
      product_version(0),
      paired(false),
      connected(false),
      uuids() {
}

BluetoothDevice::BluetoothDevice(
    const std::string& instance_id,
    const base::string16& name,
    uint32 device_class,
    device::BluetoothDevice::VendorIDSource vendor_id_source,
    uint16 vendor_id,
    uint16 product_id,
    uint16 product_version,
    bool paired,
    bool connected,
    const std::vector<std::string>& uuids)
    : instance_id(instance_id),
      name(name),
      device_class(device_class),
      vendor_id_source(vendor_id_source),
      vendor_id(vendor_id),
      product_id(product_id),
      product_version(product_version),
      paired(paired),
      connected(connected),
      uuids(uuids) {
}

BluetoothDevice::~BluetoothDevice() {
}

// static
std::vector<std::string> BluetoothDevice::UUIDsFromBluetoothUUIDs(
    const device::BluetoothDevice::UUIDList& uuid_list) {
  std::vector<std::string> uuids;
  uuids.reserve(uuid_list.size());
  for (const auto& it : uuid_list)
    uuids.push_back(it.canonical_value());
  return uuids;
}

}  // namespace content
