// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_DEVICE_H
#define DEVICE_VR_OPENVR_DEVICE_H

#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {
class IVRSystem;
}  // namespace vr

namespace device {

class OpenVRDevice : public VRDevice {
 public:
  OpenVRDevice(vr::IVRSystem* vr);
  ~OpenVRDevice() override;

  // VRDevice
  void CreateVRDisplayInfo(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) override;

  void RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                      const base::Callback<void(bool)>& callback) override;
  void SetSecureOrigin(bool secure_origin) override;
  void ExitPresent() override;

  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;
  void UpdateLayerBounds(int16_t frame_index,
                         mojom::VRLayerBoundsPtr left_bounds,
                         mojom::VRLayerBoundsPtr right_bounds,
                         int16_t source_width,
                         int16_t source_height) override;
  void GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) override;

  void OnPollingEvents();

 private:
  class OpenVRRenderLoop : public base::SimpleThread,
                           device::mojom::VRVSyncProvider {
   public:
    OpenVRRenderLoop(vr::IVRSystem* vr);

    void RegisterPollingEventCallback(
        const base::Callback<void()>& onPollingEvents);

    void UnregisterPollingEventCallback();

    void Bind(mojom::VRVSyncProviderRequest request);

    mojom::VRPosePtr getPose();

   private:
    void Run() override;

    void GetVSync(const device::mojom::VRVSyncProvider::GetVSyncCallback&
                      callback) override;

    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
    base::Callback<void()> on_polling_events_;
    vr::IVRSystem* vr_system_;
    mojo::Binding<device::mojom::VRVSyncProvider> binding_;
  };

  std::unique_ptr<OpenVRRenderLoop>
      render_loop_;  // TODO (BillOrr): This should not be a unique_ptr because
                     // the render_loop_ binds to VRVSyncProvider requests,
                     // so its lifetime should be tied to the lifetime of that
                     // binding.

  mojom::VRSubmitFrameClientPtr submit_client_;

  vr::IVRSystem* vr_system_;

  bool is_polling_events_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_H