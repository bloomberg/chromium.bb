// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/compositor_base.h"

#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/transform.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

mojom::XRFrameDataPtr XRDeviceAbstraction::GetNextFrameData() {
  return nullptr;
}
mojom::XRGamepadDataPtr XRDeviceAbstraction::GetNextGamepadData() {
  return nullptr;
}
void XRDeviceAbstraction::OnSessionStart() {}
void XRDeviceAbstraction::HandleDeviceLost() {}
bool XRDeviceAbstraction::PreComposite() {
  return true;
};
void XRDeviceAbstraction::OnLayerBoundsChanged() {}

XRCompositorCommon::XRCompositorCommon()
    : base::Thread("WindowsXRCompositor"),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      presentation_binding_(this),
      frame_data_binding_(this),
      gamepad_provider_(this) {
  DCHECK(main_thread_task_runner_);
}

XRCompositorCommon::~XRCompositorCommon() {
  // Since we derive from base::Thread, all derived classes must call Stop() in
  // their destructor so the thread gets torn down before any members that may
  // be in use on the thread get torn down.
}

void XRCompositorCommon::ClearPendingFrame() {
  has_outstanding_frame_ = false;
  if (delayed_get_frame_data_callback_) {
    base::ResetAndReturn(&delayed_get_frame_data_callback_).Run();
  }
}

void XRCompositorCommon::SubmitFrameMissing(int16_t frame_index,
                                            const gpu::SyncToken& sync_token) {
  // Nothing to do. It's OK to start the next frame even if the current
  // one didn't get sent to the runtime.
  ClearPendingFrame();
}

void XRCompositorCommon::SubmitFrame(int16_t frame_index,
                                     const gpu::MailboxHolder& mailbox,
                                     base::TimeDelta time_waited) {
  NOTREACHED();
}

void XRCompositorCommon::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  // Not currently implemented for Windows.
  NOTREACHED();
}

void XRCompositorCommon::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::ScopedHandle texture_handle) {
  TRACE_EVENT1("gpu", "SubmitFrameWithTextureHandle", "frameIndex",
               frame_index);
#if defined(OS_WIN)
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result = MojoUnwrapPlatformHandle(texture_handle.release().value(),
                                               nullptr, &platform_handle);
  if (result != MOJO_RESULT_OK) {
    ClearPendingFrame();
    return;
  }

  texture_helper_.SetSourceTexture(
      base::win::ScopedHandle(reinterpret_cast<HANDLE>(platform_handle.value)));

  bool copy_successful =
      PreComposite() && texture_helper_.CopyTextureToBackBuffer(true);
  if (copy_successful) {
    if (!SubmitCompositedFrame()) {
      ExitPresent();
    }
  }

  // Tell WebVR that we are done with the texture.
  submit_client_->OnSubmitFrameTransferred(copy_successful);
  submit_client_->OnSubmitFrameRendered();
#endif

  ClearPendingFrame();
}

void XRCompositorCommon::CleanUp() {
  submit_client_ = nullptr;
  presentation_binding_.Close();
  frame_data_binding_.Close();
  gamepad_provider_.Close();
  StopRuntime();
}

void XRCompositorCommon::RequestGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest request) {
  gamepad_provider_.Close();
  gamepad_provider_.Bind(std::move(request));
}

void XRCompositorCommon::UpdateLayerBounds(int16_t frame_id,
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

  OnLayerBoundsChanged();
};

void XRCompositorCommon::RequestSession(
    base::OnceCallback<void()> on_presentation_ended,
    mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback) {
  DCHECK(options->immersive);
  presentation_binding_.Close();
  frame_data_binding_.Close();

  if (!StartRuntime()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
    return;
  }

  DCHECK(!on_presentation_ended_);
  on_presentation_ended_ = std::move(on_presentation_ended);

  device::mojom::XRPresentationProviderPtr presentation_provider;
  device::mojom::XRFrameDataProviderPtr frame_data_provider;
  presentation_binding_.Bind(mojo::MakeRequest(&presentation_provider));
  frame_data_binding_.Bind(mojo::MakeRequest(&frame_data_provider));

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->transport_method =
      device::mojom::XRPresentationTransportMethod::SUBMIT_AS_TEXTURE_HANDLE;
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;

  OnSessionStart();

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->provider = presentation_provider.PassInterface();
  submit_frame_sink->client_request = mojo::MakeRequest(&submit_client_);
  submit_frame_sink->transport_options = std::move(transport_options);

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_provider.PassInterface();
  session->submit_frame_sink = std::move(submit_frame_sink);

  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, std::move(session)));
  is_presenting_ = true;
}

void XRCompositorCommon::ExitPresent() {
  is_presenting_ = false;
  presentation_binding_.Close();
  frame_data_binding_.Close();
  submit_client_ = nullptr;
  StopRuntime();

  has_outstanding_frame_ = false;
  delayed_get_frame_data_callback_.Reset();

  // Push out one more controller update so we don't have stale controllers.
  UpdateControllerState();

  if (on_presentation_ended_) {
    main_thread_task_runner_->PostTask(FROM_HERE,
                                       std::move(on_presentation_ended_));
  }
}

void XRCompositorCommon::UpdateControllerState() {
  if (!gamepad_callback_) {
    // Nobody is listening to updates, so bail early.
    return;
  }

  if (!is_presenting_) {
    std::move(gamepad_callback_).Run(nullptr);
    return;
  }

  std::move(gamepad_callback_).Run(GetNextGamepadData());
}

void XRCompositorCommon::RequestUpdate(
    mojom::IsolatedXRGamepadProvider::RequestUpdateCallback callback) {
  // Save the callback to resolve next time we update (typically on vsync).
  gamepad_callback_ = std::move(callback);

  // If we aren't presenting, reply now saying that we have no controllers.
  if (!is_presenting_) {
    UpdateControllerState();
  }
}

void XRCompositorCommon::Init() {}

void XRCompositorCommon::GetFrameData(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  DCHECK(is_presenting_);

  if (has_outstanding_frame_) {
    DCHECK(!delayed_get_frame_data_callback_);
    delayed_get_frame_data_callback_ =
        base::BindOnce(&XRCompositorCommon::GetFrameData,
                       base::Unretained(this), std::move(callback));
    return;
  }

  has_outstanding_frame_ = true;

  // Yield here to let the event queue process pending mojo messages,
  // specifically the next gamepad callback request that's likely to
  // have been sent during WaitGetPoses.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&XRCompositorCommon::GetControllerDataAndSendFrameData,
                     base::Unretained(this), std::move(callback),
                     GetNextFrameData()));

  next_frame_id_ += 1;
  if (next_frame_id_ < 0) {
    next_frame_id_ = 0;
  }
}

void XRCompositorCommon::GetControllerDataAndSendFrameData(
    XRFrameDataProvider::GetFrameDataCallback callback,
    mojom::XRFrameDataPtr frame_data) {
  // Update gamepad controllers.
  UpdateControllerState();

  std::move(callback).Run(std::move(frame_data));
}

}  // namespace device
