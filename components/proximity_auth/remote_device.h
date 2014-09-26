// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
#define COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H

#include <string>
#include <vector>

namespace proximity_auth {

struct RemoteDevice {
  std::string name;
  std::string bluetooth_address;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_H
