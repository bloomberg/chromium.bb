// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/bluetooth/bluetooth_device.h"

#include "base/strings/string_util.h"

namespace content {

BluetoothDevice::BluetoothDevice()
    : id(""),
      name(base::string16()),
      tx_power(device::BluetoothDevice::kUnknownPower),
      rssi(device::BluetoothDevice::kUnknownPower),
      device_class(0),
      vendor_id_source(
          device::BluetoothDevice::VendorIDSource::VENDOR_ID_UNKNOWN),
      vendor_id(0),
      product_id(0),
      product_version(0),
      uuids() {}

BluetoothDevice::BluetoothDevice(
    const std::string& id,
    const base::string16& name,
    int8_t tx_power,
    int8_t rssi,
    uint32_t device_class,
    device::BluetoothDevice::VendorIDSource vendor_id_source,
    uint16_t vendor_id,
    uint16_t product_id,
    uint16_t product_version,
    const std::vector<std::string>& uuids)
    : id(id),
      name(name),
      tx_power(tx_power),
      rssi(rssi),
      device_class(device_class),
      vendor_id_source(vendor_id_source),
      vendor_id(vendor_id),
      product_id(product_id),
      product_version(product_version),
      uuids(uuids) {}

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

// static
int8_t BluetoothDevice::ValidatePower(int16_t power) {
  return ((power < -127) || (power > 127)) ? BluetoothDevice::kUnknownPower
                                           : power;
}

}  // namespace content
