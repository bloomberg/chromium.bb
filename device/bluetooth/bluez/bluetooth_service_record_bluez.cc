// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"

#include "base/values.h"

namespace bluez {

BluetoothServiceRecordBlueZ::BluetoothServiceRecordBlueZ() {}

BluetoothServiceRecordBlueZ::~BluetoothServiceRecordBlueZ() {}

std::vector<uint16_t> BluetoothServiceRecordBlueZ::GetAttributeIds() {
  // TODO(rkc): Implement this.
  return std::vector<uint16_t>();
}

base::Value* BluetoothServiceRecordBlueZ::GetAttributeValue(
    uint16_t attribute_id) {
  // TODO(rkc): Implement this.
  return nullptr;
}

}  // namespace bluez
