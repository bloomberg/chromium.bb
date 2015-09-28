// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
#define COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H

#include <string>

namespace proximity_auth {

struct RemoteDevice {
 public:
  enum BluetoothType { BLUETOOTH_CLASSIC, BLUETOOTH_LE };

  std::string user_id;
  std::string name;
  std::string public_key;
  BluetoothType bluetooth_type;
  std::string bluetooth_address;
  std::string persistent_symmetric_key;
  std::string sign_in_challenge;

  RemoteDevice();
  RemoteDevice(const std::string& user_id,
               const std::string& name,
               const std::string& public_key,
               BluetoothType bluetooth_type,
               const std::string& bluetooth_address,
               const std::string& persistent_symmetric_key,
               std::string sign_in_challenge);
  ~RemoteDevice();
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
