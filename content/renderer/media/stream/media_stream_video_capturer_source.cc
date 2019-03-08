// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "media/capture/video_capturer_source.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    std::unique_ptr<media::VideoCapturerSource> source)
    : source_(std::move(source)) {
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* render_frame = RenderFrame::FromWebFrame(web_frame);
  render_frame_id_ =
      render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE;
  media::VideoCaptureFormats preferred_formats = source_->GetPreferredFormats();
  if (!preferred_formats.empty())
    capture_params_.requested_format = preferred_formats.front();
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    int render_frame_id,
    const SourceStoppedCallback& stop_callback,
    const blink::MediaStreamDevice& device,
    const media::VideoCaptureParams& capture_params,
    DeviceCapturerFactoryCallback device_capturer_factory_callback)
    : render_frame_id_(render_frame_id),
      source_(device_capturer_factory_callback.Run(device.session_id)),
      capture_params_(capture_params),
      device_capturer_factory_callback_(
          std::move(device_capturer_factory_callback)) {
  SetStopCallback(stop_callback);
  SetDevice(device);
  SetDeviceRotationDetection(true /* enabled */);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaStreamVideoCapturerSource::SetDeviceCapturerFactoryCallbackForTesting(
    DeviceCapturerFactoryCallback testing_factory_callback) {
  device_capturer_factory_callback_ = std::move(testing_factory_callback);
}

void MediaStreamVideoCapturerSource::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_->RequestRefreshFrame();
}

void MediaStreamVideoCapturerSource::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_->OnFrameDropped(reason);
}

void MediaStreamVideoCapturerSource::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_->OnLog(message);
}

void MediaStreamVideoCapturerSource::OnHasConsumers(bool has_consumers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_consumers)
    source_->Resume();
  else
    source_->MaybeSuspend();
}

void MediaStreamVideoCapturerSource::OnCapturingLinkSecured(bool is_secure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RenderFrame* render_frame = RenderFrame::FromRoutingID(render_frame_id_);
  if (!render_frame)
    return;
  GetMediaStreamDispatcherHost(render_frame)
      ->SetCapturingLinkSecured(device().session_id, device().type, is_secure);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const blink::VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = STARTING;
  frame_callback_ = frame_callback;
  source_->StartCapture(
      capture_params_, frame_callback_,
      base::Bind(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                 base::Unretained(this), capture_params_));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_->StopCapture();
}

void MediaStreamVideoCapturerSource::StopSourceForRestartImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STARTED) {
    OnStopForRestartDone(false);
    return;
  }
  state_ = STOPPING_FOR_RESTART;
  source_->StopCapture();

  // Force state update for nondevice sources, since they do not
  // automatically update state after StopCapture().
  if (device().type == blink::MEDIA_NO_SERVICE)
    OnRunStateChanged(capture_params_, false);
}

void MediaStreamVideoCapturerSource::RestartSourceImpl(
    const media::VideoCaptureFormat& new_format) {
  DCHECK(new_format.IsValid());
  media::VideoCaptureParams new_capture_params = capture_params_;
  new_capture_params.requested_format = new_format;
  state_ = RESTARTING;
  source_->StartCapture(
      new_capture_params, frame_callback_,
      base::Bind(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                 base::Unretained(this), new_capture_params));
}

base::Optional<media::VideoCaptureFormat>
MediaStreamVideoCapturerSource::GetCurrentFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return capture_params_.requested_format;
}

base::Optional<media::VideoCaptureParams>
MediaStreamVideoCapturerSource::GetCurrentCaptureParams() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return capture_params_;
}

void MediaStreamVideoCapturerSource::ChangeSourceImpl(
    const blink::MediaStreamDevice& new_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_capturer_factory_callback_);

  if (state_ != STARTED) {
    return;
  }

  state_ = STOPPING_FOR_CHANGE_SOURCE;
  source_->StopCapture();
  SetDevice(new_device);
  source_ = device_capturer_factory_callback_.Run(new_device.session_id);
  source_->StartCapture(
      capture_params_, frame_callback_,
      base::BindRepeating(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                          base::Unretained(this), capture_params_));
}

void MediaStreamVideoCapturerSource::OnRunStateChanged(
    const media::VideoCaptureParams& new_capture_params,
    bool is_running) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case STARTING:
      source_->OnLog("MediaStreamVideoCapturerSource sending OnStartDone");
      if (is_running) {
        state_ = STARTED;
        DCHECK(capture_params_ == new_capture_params);
        OnStartDone(blink::MEDIA_DEVICE_OK);
      } else {
        state_ = STOPPED;
        OnStartDone(blink::MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO);
      }
      break;
    case STARTED:
      if (!is_running) {
        state_ = STOPPED;
        StopSource();
      }
      break;
    case STOPPING_FOR_RESTART:
      source_->OnLog(
          "MediaStreamVideoCapturerSource sending OnStopForRestartDone");
      state_ = is_running ? STARTED : STOPPED;
      OnStopForRestartDone(!is_running);
      break;
    case STOPPING_FOR_CHANGE_SOURCE:
      state_ = is_running ? STARTED : STOPPED;
      break;
    case RESTARTING:
      if (is_running) {
        state_ = STARTED;
        capture_params_ = new_capture_params;
      } else {
        state_ = STOPPED;
      }
      source_->OnLog("MediaStreamVideoCapturerSource sending OnRestartDone");
      OnRestartDone(is_running);
      break;
    case STOPPED:
      break;
  }
}

const blink::mojom::MediaStreamDispatcherHostPtr&
MediaStreamVideoCapturerSource::GetMediaStreamDispatcherHost(
    RenderFrame* render_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dispatcher_host_) {
    render_frame->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&dispatcher_host_));
  }
  return dispatcher_host_;
}

}  // namespace content
