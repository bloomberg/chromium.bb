// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_DEVICE_H
#define DEVICE_VR_OPENVR_DEVICE_H

#include <memory>

#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {
class IVRSystem;
}  // namespace vr

namespace device {

class OpenVRRenderLoop;

class OpenVRDevice : public VRDevice {
 public:
  OpenVRDevice(vr::IVRSystem* vr);
  ~OpenVRDevice() override;

  // VRDevice
  void CreateVRDisplayInfo(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) override;

  void RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                      mojom::VRPresentationProviderRequest request,
                      const base::Callback<void(bool)>& callback) override;
  void SetSecureOrigin(bool secure_origin) override;
  void ExitPresent() override;
  void GetNextMagicWindowPose(
      mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) override;

  void OnPollingEvents();

 private:
  // TODO (BillOrr): This should not be a unique_ptr because the render_loop_
  // binds to VRVSyncProvider requests, so its lifetime should be tied to the
  // lifetime of that binding.
  std::unique_ptr<OpenVRRenderLoop> render_loop_;

  mojom::VRSubmitFrameClientPtr submit_client_;

  vr::IVRSystem* vr_system_;

  bool is_polling_events_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_H
