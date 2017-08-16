// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace device {

class GvrDelegateProvider;
class VRDisplayImpl;

class DEVICE_VR_EXPORT GvrDevice : public VRDevice {
 public:
  GvrDevice();
  ~GvrDevice() override;

  // VRDevice
  void CreateVRDisplayInfo(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) override;

  void RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                      mojom::VRPresentationProviderRequest request,
                      const base::Callback<void(bool)>& callback) override;
  void ExitPresent() override;
  void GetNextMagicWindowPose(
      VRDisplayImpl* display,
      mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) override;
  void OnDisplayAdded(VRDisplayImpl* display) override;
  void OnDisplayRemoved(VRDisplayImpl* display) override;
  void OnListeningForActivateChanged(VRDisplayImpl* display) override;

 private:
  GvrDelegateProvider* GetGvrDelegateProvider();

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
