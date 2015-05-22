// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_REMOTE_ATTRIBUTE_H
#define COMPONENTS_PROXIMITY_AUTH_BLE_REMOTE_ATTRIBUTE_H

#include <string>

#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

// Represents an attribute in the peripheral (service or characteristic).
struct RemoteAttribute {
  device::BluetoothUUID uuid;
  std::string id;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_REMOTE_ATTRIBUTE_H
