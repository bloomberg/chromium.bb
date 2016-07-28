// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_CLIENT_DISPATCHER_H
#define DEVICE_VR_VR_CLIENT_DISPATCHER_H

#include "device/vr/vr_service.mojom.h"

namespace device {

class VRClientDispatcher {
 public:
  virtual void OnDeviceChanged(VRDisplayPtr device) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_VR_CLIENT_DISPATCHER_H
