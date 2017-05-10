// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

namespace content {

namespace {

void ConcludeLaunchDeviceWithSuccess(
    bool abort_requested,
    const media::VideoCaptureParams& params,
    video_capture::mojom::DevicePtr device,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  if (abort_requested) {
    device.reset();
    callbacks->OnDeviceLaunchAborted();
    base::ResetAndReturn(&done_cb).Run();
    return;
  }

  auto receiver_adapter =
      base::MakeUnique<video_capture::ReceiverMediaToMojoAdapter>(
          base::MakeUnique<media::VideoFrameReceiverOnTaskRunner>(
              std::move(receiver),
              BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));
  video_capture::mojom::ReceiverPtr receiver_proxy;
  mojo::MakeStrongBinding<video_capture::mojom::Receiver>(
      std::move(receiver_adapter), mojo::MakeRequest(&receiver_proxy));
  device->Start(params, std::move(receiver_proxy));
  callbacks->OnDeviceLaunched(
      base::MakeUnique<ServiceLaunchedVideoCaptureDevice>(std::move(device)));
  base::ResetAndReturn(&done_cb).Run();
}

void ConcludeLaunchDeviceWithFailure(
    bool abort_requested,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  if (abort_requested)
    callbacks->OnDeviceLaunchAborted();
  else
    callbacks->OnDeviceLaunchFailed();
  base::ResetAndReturn(&done_cb).Run();
}

}  // anonymous namespace

ServiceVideoCaptureDeviceLauncher::ServiceVideoCaptureDeviceLauncher(
    video_capture::mojom::DeviceFactoryPtr* device_factory)
    : device_factory_(device_factory), state_(State::READY_TO_LAUNCH) {}

ServiceVideoCaptureDeviceLauncher::~ServiceVideoCaptureDeviceLauncher() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);

  if (stream_type != content::MEDIA_DEVICE_VIDEO_CAPTURE) {
    // This launcher only supports MEDIA_DEVICE_VIDEO_CAPTURE.
    NOTREACHED();
    return;
  }

  if (!device_factory_->is_bound()) {
    // This can happen when the ServiceVideoCaptureProvider owning
    // |device_factory_| loses connection to the service process and resets
    // |device_factory_|.
    ConcludeLaunchDeviceWithFailure(false, callbacks, std::move(done_cb));
    return;
  }
  video_capture::mojom::DevicePtr device;
  (*device_factory_)
      ->CreateDevice(
          device_id, mojo::MakeRequest(&device),
          base::Bind(
              // Use of Unretained |this| is safe, because |done_cb| guarantees
              // that |this| stays alive.
              &ServiceVideoCaptureDeviceLauncher::OnCreateDeviceCallback,
              base::Unretained(this), params, base::Passed(&device),
              std::move(receiver), callbacks, base::Passed(&done_cb)));
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void ServiceVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

void ServiceVideoCaptureDeviceLauncher::OnCreateDeviceCallback(
    const media::VideoCaptureParams& params,
    video_capture::mojom::DevicePtr device,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    Callbacks* callbacks,
    base::OnceClosure done_cb,
    video_capture::mojom::DeviceAccessResultCode result_code) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  const bool abort_requested = (state_ == State::DEVICE_START_ABORTING);
  state_ = State::READY_TO_LAUNCH;
  switch (result_code) {
    case video_capture::mojom::DeviceAccessResultCode::SUCCESS:
      ConcludeLaunchDeviceWithSuccess(abort_requested, params,
                                      std::move(device), std::move(receiver),
                                      callbacks, std::move(done_cb));
      return;
    case video_capture::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:
    case video_capture::mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      ConcludeLaunchDeviceWithFailure(abort_requested, callbacks,
                                      std::move(done_cb));
      return;
  }
}

}  // namespace content
