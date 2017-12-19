// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_render_loop.h"

#include "device/vr/oculus/oculus_type_converters.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "third_party/libovr/src/Include/OVR_CAPI_D3D.h"
#include "ui/gfx/geometry/angle_conversions.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {
OculusRenderLoop::OculusRenderLoop(ovrSession session)
    : base::Thread("OculusRenderLoop"),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      session_(session),
      binding_(this),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_task_runner_);
}

OculusRenderLoop::~OculusRenderLoop() {}

void OculusRenderLoop::SubmitFrame(int16_t frame_index,
                                   const gpu::MailboxHolder& mailbox,
                                   base::TimeDelta time_waited) {
  // There are two submit paths - one for Android, and one for Windows. This
  // method is called on Android, but this class isn't used on Android.
  NOTREACHED();
}

void OculusRenderLoop::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::ScopedHandle texture_handle) {
  TRACE_EVENT1("gpu", "SubmitFrameWithTextureHandle", "frameIndex",
               frame_index);

#if defined(OS_WIN)
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result = MojoUnwrapPlatformHandle(texture_handle.release().value(),
                                               &platform_handle);
  if (result != MOJO_RESULT_OK)
    return;

  texture_helper_.SetSourceTexture(
      base::win::ScopedHandle(reinterpret_cast<HANDLE>(platform_handle.value)));

  // Create swap chain on demand.
  if (!texture_swap_chain_) {
    ovrTextureSwapChainDesc desc = {};
    desc.Type = ovrTexture_2D;
    desc.ArraySize = 1;
    // TODO(billorr): Use SRGB, and do color conversion when we copy.
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM;
    desc.Width = source_size_.width();
    desc.Height = source_size_.height();
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.StaticImage = ovrFalse;
    desc.MiscFlags = ovrTextureMisc_None;
    desc.BindFlags = ovrTextureBind_DX_RenderTarget;
    ovr_CreateTextureSwapChainDX(session_, texture_helper_.GetDevice().Get(),
                                 &desc, &texture_swap_chain_);
  }

  bool copy_succeeded = false;
  if (texture_swap_chain_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    ovr_GetTextureSwapChainBufferDX(
        session_, texture_swap_chain_, -1,
        IID_PPV_ARGS(texture.ReleaseAndGetAddressOf()));
    texture_helper_.SetBackbuffer(texture);
    if (texture_helper_.CopyTextureToBackBuffer(false)) {
      copy_succeeded = true;
      ovrLayerEyeFov layer = {};
      layer.Header.Type = ovrLayerType_EyeFov;
      layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
      layer.ColorTexture[0] = texture_swap_chain_;
      DCHECK(source_size_.width() % 2 == 0);
      layer.Viewport[0] = {
          {source_size_.width() * left_bounds_.x(),
           source_size_.height() * left_bounds_.y()},
          {source_size_.width() * left_bounds_.width(),
           source_size_.height() * left_bounds_.height()}};  // left viewport
      layer.Viewport[1] = {
          {source_size_.width() * right_bounds_.x(),
           source_size_.height() * right_bounds_.y()},
          {source_size_.width() * right_bounds_.width(),
           source_size_.height() * right_bounds_.height()}};  // right viewport
      ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session_);
      layer.Fov[0] = hmdDesc.DefaultEyeFov[0];
      layer.Fov[1] = hmdDesc.DefaultEyeFov[1];

      auto render_desc_left = ovr_GetRenderDesc(
          session_, ovrEye_Left, hmdDesc.DefaultEyeFov[ovrEye_Left]);
      auto render_desc_right = ovr_GetRenderDesc(
          session_, ovrEye_Right, hmdDesc.DefaultEyeFov[ovrEye_Right]);
      ovrVector3f eye_offsets[2] = {render_desc_left.HmdToEyeOffset,
                                    render_desc_right.HmdToEyeOffset};
      ovr_CalcEyePoses(last_render_pose_, eye_offsets, layer.RenderPose);

      layer.SensorSampleTime = sensor_time_;

      ovrViewScaleDesc view_scale_desc;
      view_scale_desc.HmdToEyeOffset[ovrEye_Left] = eye_offsets[ovrEye_Left];
      view_scale_desc.HmdToEyeOffset[ovrEye_Right] = eye_offsets[ovrEye_Right];
      view_scale_desc.HmdSpaceToWorldScaleInMeters = 1;

      constexpr unsigned int layer_count = 1;
      ovrLayerHeader* layer_headers[layer_count] = {&layer.Header};
      ovr_CommitTextureSwapChain(session_, texture_swap_chain_);
      ovrResult result =
          ovr_SubmitFrame(session_, ovr_frame_index_, &view_scale_desc,
                          layer_headers, layer_count);
      DCHECK(OVR_SUCCESS(result));
      ovr_frame_index_++;
    }
  }
  // Tell WebVR that we are done with the texture.
  submit_client_->OnSubmitFrameTransferred(copy_succeeded);
  submit_client_->OnSubmitFrameRendered();
#endif
}

void OculusRenderLoop::UpdateLayerBounds(int16_t frame_id,
                                         const gfx::RectF& left_bounds,
                                         const gfx::RectF& right_bounds,
                                         const gfx::Size& source_size) {
  // There is no parallelism, so we can ignore frame_id and assume the bounds
  // are updating for the next frame.
  left_bounds_ = left_bounds;
  right_bounds_ = right_bounds;
  source_size_ = source_size;

  // TODO(billorr): Recreate texture_swap_chain_ when source_size_ has changed.
};

void OculusRenderLoop::RequestPresent(
    mojom::VRSubmitFrameClientPtrInfo submit_client_info,
    mojom::VRPresentationProviderRequest request,
    base::OnceCallback<void(bool)> callback) {
#if defined(OS_WIN)
  if (!texture_helper_.EnsureInitialized()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
#endif
  submit_client_.Bind(std::move(submit_client_info));

  binding_.Close();
  binding_.Bind(std::move(request));

  main_thread_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), true));
  is_presenting_ = true;
}

void OculusRenderLoop::ExitPresent() {
  is_presenting_ = false;
}

void OculusRenderLoop::Init() {}

base::WeakPtr<OculusRenderLoop> OculusRenderLoop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OculusRenderLoop::GetVSync(
    mojom::VRPresentationProvider::GetVSyncCallback callback) {
  int16_t frame = next_frame_id_++;
  DCHECK(is_presenting_);

  auto predicted_time =
      ovr_GetPredictedDisplayTime(session_, ovr_frame_index_ + 1);
  ovrTrackingState state = ovr_GetTrackingState(session_, predicted_time, true);
  sensor_time_ = ovr_GetTimeInSeconds();
  mojom::VRPosePtr pose =
      mojo::ConvertTo<mojom::VRPosePtr>(state.HeadPose.ThePose);
  last_render_pose_ = state.HeadPose.ThePose;

  base::TimeDelta time = base::TimeDelta::FromSecondsD(predicted_time);

  std::move(callback).Run(std::move(pose), time, frame,
                          mojom::VRPresentationProvider::VSyncStatus::SUCCESS);
}

}  // namespace device
