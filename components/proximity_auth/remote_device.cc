// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

RemoteDevice::RemoteDevice() : bluetooth_type(BLUETOOTH_CLASSIC) {}

RemoteDevice::RemoteDevice(const std::string& user_id,
                           const std::string& name,
                           const std::string& public_key,
                           BluetoothType bluetooth_type,
                           const std::string& bluetooth_address,
                           const std::string& persistent_symmetric_key,
                           std::string sign_in_challenge)
    : user_id(user_id),
      name(name),
      public_key(public_key),
      bluetooth_type(bluetooth_type),
      bluetooth_address(bluetooth_address),
      persistent_symmetric_key(persistent_symmetric_key),
      sign_in_challenge(sign_in_challenge) {}

RemoteDevice::~RemoteDevice() {}

}  // namespace
