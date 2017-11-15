// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_RENDER_LOOP_H
#define DEVICE_VR_OPENVR_RENDER_LOOP_H

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

class OpenVRRenderLoop : public base::Thread, mojom::VRPresentationProvider {
 public:
  OpenVRRenderLoop();
  ~OpenVRRenderLoop() override;

  void RequestPresent(mojom::VRSubmitFrameClientPtrInfo submit_client_info,
                      mojom::VRPresentationProviderRequest request,
                      base::Callback<void(bool)> callback);
  void ExitPresent();
  base::WeakPtr<OpenVRRenderLoop> GetWeakPtr();

  // VRPresentationProvider overrides:
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::ScopedHandle texture_handle) override;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;
  void GetVSync(GetVSyncCallback callback) override;

 private:
  // base::Thread overrides:
  void Init() override;

  mojom::VRPosePtr GetPose();

#if defined(OS_WIN)
  D3D11TextureHelper texture_helper_;
#endif

  int16_t next_frame_id_ = 0;
  bool is_presenting_ = false;
  gfx::RectF left_bounds_;
  gfx::RectF right_bounds_;
  gfx::Size source_size_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  vr::IVRCompositor* vr_compositor_;
  mojom::VRSubmitFrameClientPtr submit_client_;
  mojo::Binding<mojom::VRPresentationProvider> binding_;
  base::WeakPtrFactory<OpenVRRenderLoop> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_RENDER_LOOP_H