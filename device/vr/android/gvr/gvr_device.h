// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_device.h"

namespace device {

class GvrDeviceProvider;
class GvrDelegate;

class DEVICE_VR_EXPORT GvrDevice : public VRDevice {
 public:
  GvrDevice(GvrDeviceProvider* provider);
  ~GvrDevice() override;

  // VRDevice
  void GetVRDevice(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& callback) override;
  void ResetPose() override;

  void RequestPresent(const base::Callback<void(bool)>& callback) override;
  void SetSecureOrigin(bool secure_origin) override;
  void ExitPresent() override;

  void SubmitFrame(mojom::VRPosePtr pose) override;
  void UpdateLayerBounds(int16_t frame_index,
                         mojom::VRLayerBoundsPtr left_bounds,
                         mojom::VRLayerBoundsPtr right_bounds) override;
  void GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) override;
  void OnDelegateChanged();

 private:
  GvrDelegate* GetGvrDelegate();

  GvrDeviceProvider* gvr_provider_;
  bool secure_origin_ = false;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
