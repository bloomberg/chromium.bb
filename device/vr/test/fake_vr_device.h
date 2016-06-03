// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
#define DEVICE_VR_TEST_FAKE_VR_DEVICE_H_

#include "base/macros.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"

namespace device {

class FakeVRDevice : public VRDevice {
 public:
  explicit FakeVRDevice(VRDeviceProvider* provider);
  ~FakeVRDevice() override;

  void SetVRDevice(const blink::mojom::VRDisplayPtr& device);
  void SetPose(const blink::mojom::VRPosePtr& state);

  blink::mojom::VRDisplayPtr GetVRDevice() override;
  blink::mojom::VRPosePtr GetPose() override;
  void ResetPose() override;

 private:
  blink::mojom::VRDisplayPtr device_;
  blink::mojom::VRPosePtr pose_;

  DISALLOW_COPY_AND_ASSIGN(FakeVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
