// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/bluetooth/bluetooth_device.h"

#include "base/strings/string_util.h"

namespace content {

BluetoothDevice::BluetoothDevice()
    : id(""),
      name(base::string16()),
      uuids() {}

BluetoothDevice::BluetoothDevice(
    const std::string& id,
    const base::string16& name,
    const std::vector<std::string>& uuids)
    : id(id),
      name(name),
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

}  // namespace content
