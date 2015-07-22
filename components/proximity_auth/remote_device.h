// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
#define COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H

#include <string>

namespace proximity_auth {

struct RemoteDevice {
 public:
  std::string name;
  std::string public_key;
  std::string bluetooth_address;
  std::string persistent_symmetric_key;

  RemoteDevice();
  RemoteDevice(const std::string& name,
               const std::string& public_key,
               const std::string& bluetooth_address,
               const std::string& persistent_symmetric_key);
  ~RemoteDevice();
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
