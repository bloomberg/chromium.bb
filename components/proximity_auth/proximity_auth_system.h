// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H

#include "base/macros.h"
#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

// This is the main entry point to start Proximity Auth, the underlying system
// for the Easy Unlock and Easy Sign-in features. Given a list of registered
// remote devices (i.e. phones), this object will handle the connection,
// authentication, and protocol for all the devices.
class ProximityAuthSystem {
 public:
  ProximityAuthSystem(const std::vector<RemoteDevice>& remote_devices);
  virtual ~ProximityAuthSystem();

  const std::vector<RemoteDevice>& GetRemoteDevices();

 private:
  std::vector<RemoteDevice> remote_devices_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
