// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace device {

class GvrDelegate;

class GvrDevice : public VRDevice {
 public:
  GvrDevice(VRDeviceProvider* provider, GvrDelegate* delegate);
  ~GvrDevice() override;

  // VRDevice
  VRDisplayPtr GetVRDevice() override;
  VRPosePtr GetPose() override;
  void ResetPose() override;

  void RequestPresent() override;
  void ExitPresent() override;

  void SubmitFrame() override;
  void UpdateLayerBounds(VRLayerBoundsPtr leftBounds,
                         VRLayerBoundsPtr rightBounds) override;

 private:
  GvrDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
