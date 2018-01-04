// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_render_loop.h"

#include "device/vr/openvr/openvr_type_converters.h"
#include "ui/gfx/geometry/angle_conversions.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {
OpenVRRenderLoop::OpenVRRenderLoop()
    : base::Thread("OpenVRRenderLoop"),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_task_runner_);
}

OpenVRRenderLoop::~OpenVRRenderLoop() {
  Stop();
}

void OpenVRRenderLoop::SubmitFrame(int16_t frame_index,
                                   const gpu::MailboxHolder& mailbox,
                                   base::TimeDelta time_waited) {
  NOTREACHED();
}

void OpenVRRenderLoop::SubmitFrameWithTextureHandle(
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
  texture_helper_.AllocateBackBuffer();
  bool copy_successful = texture_helper_.CopyTextureToBackBuffer(true);
  if (copy_successful) {
    vr::Texture_t texture;
    texture.handle = texture_helper_.GetBackbuffer().Get();
    texture.eType = vr::TextureType_DirectX;
    texture.eColorSpace = vr::ColorSpace_Auto;

    vr::VRTextureBounds_t bounds[2];
    bounds[0] = {left_bounds_.x(), left_bounds_.y(),
                 left_bounds_.width() + left_bounds_.x(),
                 left_bounds_.height() + left_bounds_.y()};
    bounds[1] = {right_bounds_.x(), right_bounds_.y(),
                 right_bounds_.width() + right_bounds_.x(),
                 right_bounds_.height() + right_bounds_.y()};

    vr::EVRCompositorError error =
        vr_compositor_->Submit(vr::EVREye::Eye_Left, &texture, &bounds[0]);
    if (error != vr::VRCompositorError_None) {
      ExitPresent();
      return;
    }
    error = vr_compositor_->Submit(vr::EVREye::Eye_Right, &texture, &bounds[1]);
    if (error != vr::VRCompositorError_None) {
      ExitPresent();
      return;
    }
    vr_compositor_->PostPresentHandoff();
  }

  // Tell WebVR that we are done with the texture.
  submit_client_->OnSubmitFrameTransferred(copy_successful);
  submit_client_->OnSubmitFrameRendered();
#endif
}

void OpenVRRenderLoop::CleanUp() {
  submit_client_ = nullptr;
  binding_.Close();
}

void OpenVRRenderLoop::UpdateLayerBounds(int16_t frame_id,
                                         const gfx::RectF& left_bounds,
                                         const gfx::RectF& right_bounds,
                                         const gfx::Size& source_size) {
  // Bounds are updated instantly, rather than waiting for frame_id.  This works
  // since blink always passes the current frame_id when updating the bounds.
  // Ignoring the frame_id keeps the logic simpler, so this can more easily
  // merge with vr_shell_gl eventually.
  left_bounds_ = left_bounds;
  right_bounds_ = right_bounds;
  source_size_ = source_size;
};

void OpenVRRenderLoop::RequestPresent(
    mojom::VRSubmitFrameClientPtrInfo submit_client_info,
    mojom::VRPresentationProviderRequest request,
    device::mojom::VRRequestPresentOptionsPtr present_options,
    device::mojom::VRDisplayHost::RequestPresentCallback callback) {
#if defined(OS_WIN)
  int32_t adapter_index;
  vr::VRSystem()->GetDXGIOutputInfo(&adapter_index);
  if (!texture_helper_.SetAdapterIndex(adapter_index) ||
      !texture_helper_.EnsureInitialized()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
    return;
  }
#endif
  submit_client_.Bind(std::move(submit_client_info));

  binding_.Close();
  binding_.Bind(std::move(request));

  device::mojom::VRDisplayFrameTransportOptionsPtr transport_options =
      device::mojom::VRDisplayFrameTransportOptions::New();
  transport_options->transport_method =
      device::mojom::VRDisplayFrameTransportMethod::SUBMIT_AS_TEXTURE_HANDLE;
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, std::move(transport_options)));
  is_presenting_ = true;
  vr_compositor_->SuspendRendering(false);
}

void OpenVRRenderLoop::ExitPresent() {
  is_presenting_ = false;
  vr_compositor_->SuspendRendering(true);
}

mojom::VRPosePtr OpenVRRenderLoop::GetPose() {
  vr::TrackedDevicePose_t rendering_poses[vr::k_unMaxTrackedDeviceCount];

  TRACE_EVENT0("gpu", "WaitGetPoses");
  vr_compositor_->WaitGetPoses(rendering_poses, vr::k_unMaxTrackedDeviceCount,
                               nullptr, 0);

  return mojo::ConvertTo<mojom::VRPosePtr>(
      rendering_poses[vr::k_unTrackedDeviceIndex_Hmd]);
}

void OpenVRRenderLoop::Init() {
  vr_compositor_ = vr::VRCompositor();
  if (vr_compositor_ == nullptr) {
    DLOG(ERROR) << "Failed to initialize compositor.";
    return;
  }

  vr_compositor_->SuspendRendering(true);
  vr_compositor_->SetTrackingSpace(
      vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
}

base::WeakPtr<OpenVRRenderLoop> OpenVRRenderLoop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OpenVRRenderLoop::GetVSync(
    mojom::VRPresentationProvider::GetVSyncCallback callback) {
  int16_t frame = next_frame_id_++;
  DCHECK(is_presenting_);

  mojom::VRPosePtr pose = GetPose();

  vr::Compositor_FrameTiming timing;
  timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
  bool valid_time = vr_compositor_->GetFrameTiming(&timing);
  base::TimeDelta time =
      valid_time ? base::TimeDelta::FromSecondsD(timing.m_flSystemTimeInSeconds)
                 : base::TimeDelta();

  std::move(callback).Run(std::move(pose), time, frame,
                          mojom::VRPresentationProvider::VSyncStatus::SUCCESS);
}

}  // namespace device
