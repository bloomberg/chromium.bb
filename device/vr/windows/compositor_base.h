// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_
#define DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

class XRDeviceAbstraction {
 public:
  virtual mojom::XRFrameDataPtr GetNextFrameData();
  virtual mojom::XRGamepadDataPtr GetNextGamepadData();
  virtual bool StartRuntime() = 0;
  virtual void StopRuntime() = 0;
  virtual void OnSessionStart();
  virtual bool PreComposite();
  virtual bool SubmitCompositedFrame() = 0;
  virtual void HandleDeviceLost();
  virtual void OnLayerBoundsChanged();
};

class XRCompositorCommon : public base::Thread,
                           public XRDeviceAbstraction,
                           public mojom::XRPresentationProvider,
                           public mojom::XRFrameDataProvider,
                           public mojom::IsolatedXRGamepadProvider {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(bool result, mojom::XRSessionPtr)>;

  XRCompositorCommon();
  ~XRCompositorCommon() override;

  void RequestSession(base::OnceCallback<void()> on_presentation_ended,
                      mojom::XRRuntimeSessionOptionsPtr options,
                      RequestSessionCallback callback);
  void ExitPresent();

  void GetFrameData(XRFrameDataProvider::GetFrameDataCallback callback) final;
  void GetControllerDataAndSendFrameData(
      XRFrameDataProvider::GetFrameDataCallback callback,
      mojom::XRFrameDataPtr frame_data);

  void RequestGamepadProvider(mojom::IsolatedXRGamepadProviderRequest request);

 protected:
  // base::Thread overrides:
  void Init() final;
  void CleanUp() final;

  void ClearPendingFrame();
  void UpdateControllerState();

  // IsolatedXRGamepadProvider
  void RequestUpdate(
      mojom::IsolatedXRGamepadProvider::RequestUpdateCallback callback) final;

  // XRPresentationProvider overrides:
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) final;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) final;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) final;
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::ScopedHandle texture_handle) final;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) final;

#if defined(OS_WIN)
  D3D11TextureHelper texture_helper_;
#endif

  base::OnceCallback<void()> delayed_get_frame_data_callback_;
  bool has_outstanding_frame_ = false;

  int16_t next_frame_id_ = 0;
  bool is_presenting_ = false;
  gfx::RectF left_bounds_;
  gfx::RectF right_bounds_;
  gfx::Size source_size_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojom::XRPresentationClientPtr submit_client_;
  base::OnceCallback<void()> on_presentation_ended_;
  mojom::IsolatedXRGamepadProvider::RequestUpdateCallback gamepad_callback_;
  mojo::Binding<mojom::XRPresentationProvider> presentation_binding_;
  mojo::Binding<mojom::XRFrameDataProvider> frame_data_binding_;
  mojo::Binding<mojom::IsolatedXRGamepadProvider> gamepad_provider_;

  DISALLOW_COPY_AND_ASSIGN(XRCompositorCommon);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_
