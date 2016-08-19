// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

class GvrDevice : public VRDevice {
 public:
  GvrDevice(VRDeviceProvider* provider, gvr::GvrApi* gvr_api);
  ~GvrDevice() override;

  // VRDevice
  VRDisplayPtr GetVRDevice() override;
  VRPosePtr GetPose() override;
  void ResetPose() override;

 private:
  gvr::GvrApi* gvr_api_;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
