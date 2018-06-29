// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
#define DEVICE_VR_TEST_FAKE_VR_DEVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/vr/vr_device_base.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"

namespace device {

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT FakeVRDevice : public VRDeviceBase,
                                      public mojom::XRSessionController {
 public:
  FakeVRDevice(unsigned int id);
  ~FakeVRDevice() override;

  void RequestSession(
      mojom::XRDeviceRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void SetPose(mojom::VRPosePtr pose) { pose_ = std::move(pose); }

  void SetFrameDataRestricted(bool restricted) override {}

  using VRDeviceBase::IsPresenting;  // Make it public for tests.

  void StopSession() { OnPresentingControllerMojoConnectionError(); }

 private:
  void OnPresentingControllerMojoConnectionError();

  void OnMagicWindowFrameDataRequest(
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback) override;

  mojom::VRDisplayInfoPtr InitBasicDevice();
  mojom::VREyeParametersPtr InitEye(float fov, float offset, uint32_t size);

  mojom::VRPosePtr pose_;

  mojo::Binding<mojom::XRSessionController> controller_binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_DEVICE_H_
