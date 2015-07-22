// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

RemoteDevice::RemoteDevice() {}

RemoteDevice::RemoteDevice(const std::string& name,
                           const std::string& public_key,
                           const std::string& bluetooth_address,
                           const std::string& persistent_symmetric_key)
    : name(name),
      public_key(public_key),
      bluetooth_address(bluetooth_address),
      persistent_symmetric_key(persistent_symmetric_key) {}

RemoteDevice::~RemoteDevice() {}

}  // namespace
