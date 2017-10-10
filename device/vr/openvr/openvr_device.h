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
  mojom::VRDisplayInfoPtr GetVRDisplayInfo() override;
  void RequestPresent(
      VRDisplayImpl* display,
      mojom::VRSubmitFrameClientPtr submit_client,
      mojom::VRPresentationProviderRequest request,
      mojom::VRDisplayHost::RequestPresentCallback callback) override;
  void ExitPresent() override;
  void GetPose(VRDisplayImpl* display,
               mojom::VRMagicWindowProvider::GetPoseCallback callback) override;

  void OnPollingEvents();

 private:
  void CreateVRDisplayInfo();

  // TODO (BillOrr): This should not be a unique_ptr because the render_loop_
  // binds to VRVSyncProvider requests, so its lifetime should be tied to the
  // lifetime of that binding.
  std::unique_ptr<OpenVRRenderLoop> render_loop_;

  mojom::VRSubmitFrameClientPtr submit_client_;
  mojom::VRDisplayInfoPtr display_info_;

  vr::IVRSystem* vr_system_;

  bool is_polling_events_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_H
