// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_
#define DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_

#include "base/synchronization/lock.h"
#include "device/vr/openvr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"

class ID3D11Texture2D;

namespace vr {

struct ProjectionRaw {
  float projection[4];
};

class TestHelper : public device::TestHookRegistration {
 public:
  // Methods called by mock OpenVR APIs.
  void OnPresentedFrame(ID3D11Texture2D* texture,
                        const VRTextureBounds_t* bounds,
                        EVREye eye);
  TrackedDevicePose_t GetPose(bool presenting);
  float GetIpd();
  ProjectionRaw GetProjectionRaw(bool left);
  void TestFailure();

  // TestHookRegistration
  void SetTestHook(device::OpenVRTestHook* hook) final;

 private:
  device::OpenVRTestHook* test_hook_ = nullptr;
  base::Lock lock_;
};

}  // namespace vr

#endif  // DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_