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

  void InitBasicDevice();

  void SetVRDevice(const mojom::VRDisplayInfoPtr& device);
  void SetPose(const mojom::VRPosePtr& state);

  mojom::VRDisplayInfoPtr GetVRDevice() override;
  mojom::VRPosePtr GetPose(VRServiceImpl* service) override;
  void ResetPose(VRServiceImpl* service) override;

  bool RequestPresent(VRServiceImpl* service, bool secure_origin) override;
  void ExitPresent(VRServiceImpl* service) override;
  void SubmitFrame(VRServiceImpl* service, mojom::VRPosePtr pose) override;
  void UpdateLayerBounds(VRServiceImpl* service,
                         mojom::VRLayerBoundsPtr leftBounds,
                         mojom::VRLayerBoundsPtr rightBounds) override;

 private:
  mojom::VREyeParametersPtr InitEye(float fov, float offset, uint32_t size);

  mojom::VRDisplayInfoPtr device_;
  mojom::VRPosePtr pose_;

  DISALLOW_COPY_AND_ASSIGN(FakeVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
