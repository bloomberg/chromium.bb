// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_socket_resource.h"

#include <string>

#include "device/bluetooth/bluetooth_socket.h"

namespace extensions {

BluetoothSocketResource::BluetoothSocketResource(
    const std::string& owner_extension_id,
    scoped_refptr<device::BluetoothSocket> socket)
    : ApiResource(owner_extension_id), socket_(socket) {}

BluetoothSocketResource::~BluetoothSocketResource() {}

}  // namespace extensions
