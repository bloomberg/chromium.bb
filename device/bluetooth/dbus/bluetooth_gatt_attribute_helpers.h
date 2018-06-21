// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTES_HELPER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTES_HELPER_H_

#include <map>

#include "dbus/object_path.h"

namespace dbus {
class MessageReader;
}

namespace bluez {

// Helper methods used from various GATT attribute providers and clients.
bool ReadOptions(dbus::MessageReader* reader,
                 std::map<std::string, dbus::MessageReader>* options);

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTES_HELPER_H_
