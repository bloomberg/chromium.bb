// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/vr_device.h"

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

class GvrDeviceProvider;
class GvrDelegate;

class GvrDevice : public VRDevice {
 public:
  GvrDevice(GvrDeviceProvider* provider,
            const base::WeakPtr<GvrDelegate>& delegate);
  ~GvrDevice() override;

  // VRDevice
  mojom::VRDisplayInfoPtr GetVRDevice() override;
  mojom::VRPosePtr GetPose(VRServiceImpl* service) override;
  void ResetPose(VRServiceImpl* service) override;

  bool RequestPresent(VRServiceImpl* service, bool secure_origin) override;
  void ExitPresent(VRServiceImpl* service) override;

  void SubmitFrame(VRServiceImpl* service, mojom::VRPosePtr pose) override;
  void UpdateLayerBounds(VRServiceImpl* service,
                         mojom::VRLayerBoundsPtr leftBounds,
                         mojom::VRLayerBoundsPtr rightBounds) override;

  void SetDelegate(const base::WeakPtr<GvrDelegate>& delegate);

 private:
  gvr::GvrApi* GetGvrApi();

  base::WeakPtr<GvrDelegate> delegate_;
  GvrDeviceProvider* gvr_provider_;
  bool secure_origin_ = false;
  uint32_t pose_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
