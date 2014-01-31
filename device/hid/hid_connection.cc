// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

namespace device {

HidConnection::HidConnection(HidDeviceInfo device_info)
    : device_info_(device_info) {}

HidConnection::~HidConnection() {}

const HidDeviceInfo& HidConnection::device_info() const {
  return device_info_;
}

}  // namespace device
