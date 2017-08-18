// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/stack_trace.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/video_capturer_source.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

// LocalVideoCapturerSource is a delegate used by MediaStreamVideoCapturerSource
// for local video capture. It uses the Render singleton VideoCaptureImplManager
// to start / stop and receive I420 frames from Chrome's video capture
// implementation. This is a main Render thread only object.
class LocalVideoCapturerSource final : public media::VideoCapturerSource {
 public:
  explicit LocalVideoCapturerSource(const StreamDeviceInfo& device_info);
  ~LocalVideoCapturerSource() override;

  // VideoCaptureDelegate Implementation.
  media::VideoCaptureFormats GetPreferredFormats() override;
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override;
  void RequestRefreshFrame() override;
  void MaybeSuspend() override;
  void Resume() override;
  void StopCapture() override;

 private:
  void OnStateUpdate(VideoCaptureState state);

  // |session_id_| identifies the capture device used for this capture session.
  const media::VideoCaptureSessionId session_id_;

  VideoCaptureImplManager* const manager_;

  const base::Closure release_device_cb_;

  // These two are valid between StartCapture() and StopCapture().
  // |running_call_back_| is run when capture is successfully started, and when
  // it is stopped or error happens.
  RunningCallback running_callback_;
  base::Closure stop_capture_cb_;

  // Bound to the main render thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<LocalVideoCapturerSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoCapturerSource);
};

LocalVideoCapturerSource::LocalVideoCapturerSource(
    const StreamDeviceInfo& device_info)
    : session_id_(device_info.session_id),
      manager_(RenderThreadImpl::current()->video_capture_impl_manager()),
      release_device_cb_(manager_->UseDevice(session_id_)),
      weak_factory_(this) {
  DCHECK(RenderThreadImpl::current());
}

LocalVideoCapturerSource::~LocalVideoCapturerSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  release_device_cb_.Run();
}

media::VideoCaptureFormats LocalVideoCapturerSource::GetPreferredFormats() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return media::VideoCaptureFormats();
}

void LocalVideoCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const RunningCallback& running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());
  running_callback_ = running_callback;

  stop_capture_cb_ =
      manager_->StartCapture(session_id_, params,
                             media::BindToCurrentLoop(base::Bind(
                                 &LocalVideoCapturerSource::OnStateUpdate,
                                 weak_factory_.GetWeakPtr())),
                             new_frame_callback);
}

void LocalVideoCapturerSource::RequestRefreshFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stop_capture_cb_.is_null())
    return;  // Do not request frames if the source is stopped.
  manager_->RequestRefreshFrame(session_id_);
}

void LocalVideoCapturerSource::MaybeSuspend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager_->Suspend(session_id_);
}

void LocalVideoCapturerSource::Resume() {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager_->Resume(session_id_);
}

void LocalVideoCapturerSource::StopCapture() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Immediately make sure we don't provide more frames.
  if (!stop_capture_cb_.is_null())
    base::ResetAndReturn(&stop_capture_cb_).Run();
  running_callback_.Reset();
}

void LocalVideoCapturerSource::OnStateUpdate(VideoCaptureState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (running_callback_.is_null())
    return;
  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      running_callback_.Run(true);
      break;

    case VIDEO_CAPTURE_STATE_STOPPING:
    case VIDEO_CAPTURE_STATE_STOPPED:
    case VIDEO_CAPTURE_STATE_ERROR:
    case VIDEO_CAPTURE_STATE_ENDED:
      base::ResetAndReturn(&running_callback_).Run(false);
      break;

    case VIDEO_CAPTURE_STATE_STARTING:
    case VIDEO_CAPTURE_STATE_PAUSED:
    case VIDEO_CAPTURE_STATE_RESUMED:
      // Not applicable to reporting on device starts or errors.
      break;
  }
}

}  // namespace

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    std::unique_ptr<media::VideoCapturerSource> source)
    : RenderFrameObserver(nullptr),
      dispatcher_host_(nullptr),
      source_(std::move(source)) {
  media::VideoCaptureFormats preferred_formats = source_->GetPreferredFormats();
  if (!preferred_formats.empty())
    capture_params_.requested_format = preferred_formats.front();
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    const StreamDeviceInfo& device_info,
    const media::VideoCaptureParams& capture_params,
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      dispatcher_host_(nullptr),
      source_(new LocalVideoCapturerSource(device_info)),
      capture_params_(capture_params) {
  SetStopCallback(stop_callback);
  SetDeviceInfo(device_info);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::RequestRefreshFrame() {
  source_->RequestRefreshFrame();
}

void MediaStreamVideoCapturerSource::OnHasConsumers(bool has_consumers) {
  if (has_consumers)
    source_->Resume();
  else
    source_->MaybeSuspend();
}

void MediaStreamVideoCapturerSource::OnCapturingLinkSecured(bool is_secure) {
  GetMediaStreamDispatcherHost()->SetCapturingLinkSecured(
      device_info().session_id, device_info().device.type, is_secure);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const VideoCaptureDeliverFrameCB& frame_callback) {
  is_capture_starting_ = true;
  source_->StartCapture(
      capture_params_, frame_callback,
      base::Bind(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                 base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  source_->StopCapture();
}

base::Optional<media::VideoCaptureFormat>
MediaStreamVideoCapturerSource::GetCurrentFormat() const {
  return base::Optional<media::VideoCaptureFormat>(
      capture_params_.requested_format);
}

void MediaStreamVideoCapturerSource::OnRunStateChanged(bool is_running) {
  if (is_capture_starting_) {
    OnStartDone(is_running ? MEDIA_DEVICE_OK
                           : MEDIA_DEVICE_TRACK_START_FAILURE);
    is_capture_starting_ = false;
  } else if (!is_running) {
    StopSource();
  }
}

mojom::MediaStreamDispatcherHost*
MediaStreamVideoCapturerSource::GetMediaStreamDispatcherHost() {
  if (!dispatcher_host_) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, &dispatcher_host_ptr_);
    dispatcher_host_ = dispatcher_host_ptr_.get();
  }
  return dispatcher_host_;
};

}  // namespace content
