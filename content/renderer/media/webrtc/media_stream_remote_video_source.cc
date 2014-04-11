// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/renderer/media/native_handle_impl.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

namespace content {

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    webrtc::VideoTrackInterface* remote_track)
    : MediaStreamVideoSource(NULL),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      remote_track_(remote_track),
      last_state_(remote_track_->state()),
      first_frame_received_(false) {
  remote_track_->AddRenderer(this);
  remote_track_->RegisterObserver(this);
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  StopSourceImpl();
}

void MediaStreamRemoteVideoSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (format_.IsValid()) {
    media::VideoCaptureFormats formats;
    formats.push_back(format_);
    OnSupportedFormats(formats);
  }
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const media::VideoCaptureParams& params) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  OnStartDone(true);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  remote_track_->RemoveRenderer(this);
  remote_track_->UnregisterObserver(this);
}

webrtc::VideoSourceInterface* MediaStreamRemoteVideoSource::GetAdapter() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  return remote_track_->GetSource();
}

void MediaStreamRemoteVideoSource::SetSize(int width, int height) {
}

void MediaStreamRemoteVideoSource::RenderFrame(
    const cricket::VideoFrame* frame) {
  base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      frame->GetTimeStamp() / talk_base::kNumNanosecsPerMillisec);

  scoped_refptr<media::VideoFrame> video_frame;
  if (frame->GetNativeHandle() != NULL) {
    NativeHandleImpl* handle =
        static_cast<NativeHandleImpl*>(frame->GetNativeHandle());
    video_frame = static_cast<media::VideoFrame*>(handle->GetHandle());
    video_frame->set_timestamp(timestamp);
  } else {
    gfx::Size size(frame->GetWidth(), frame->GetHeight());
    video_frame = frame_pool_.CreateFrame(
        media::VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);

    // Non-square pixels are unsupported.
    DCHECK_EQ(frame->GetPixelWidth(), 1u);
    DCHECK_EQ(frame->GetPixelHeight(), 1u);

    int y_rows = frame->GetHeight();
    int uv_rows = frame->GetChromaHeight();
    CopyYPlane(
        frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame.get());
    CopyUPlane(
        frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame.get());
    CopyVPlane(
        frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame.get());
  }

  if (!first_frame_received_) {
    first_frame_received_ = true;
    media::VideoPixelFormat pixel_format = (
        video_frame->format() == media::VideoFrame::YV12) ?
            media::PIXEL_FORMAT_YV12 : media::PIXEL_FORMAT_TEXTURE;
    media::VideoCaptureFormat format(
        gfx::Size(video_frame->natural_size().width(),
                  video_frame->natural_size().height()),
        MediaStreamVideoSource::kDefaultFrameRate,
        pixel_format);
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&MediaStreamRemoteVideoSource::FrameFormatOnMainThread,
                   AsWeakPtr(), format));
  }

  // TODO(perkj): Consider delivering the frame on whatever thread the frame is
  // delivered on once crbug/335327 is fixed.
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediaStreamRemoteVideoSource::DoRenderFrameOnMainThread,
                 AsWeakPtr(), video_frame));
}

void MediaStreamRemoteVideoSource::OnChanged() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  webrtc::MediaStreamTrackInterface::TrackState state = remote_track_->state();
  if (state != last_state_) {
    last_state_ = state;
    switch (state) {
      case webrtc::MediaStreamTrackInterface::kInitializing:
        // Ignore the kInitializing state since there is no match in
        // WebMediaStreamSource::ReadyState.
        break;
      case webrtc::MediaStreamTrackInterface::kLive:
        SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
        break;
      case webrtc::MediaStreamTrackInterface::kEnded:
        SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void MediaStreamRemoteVideoSource::FrameFormatOnMainThread(
    const media::VideoCaptureFormat& format) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  format_ = format;
  if (state() == RETRIEVING_CAPABILITIES) {
    media::VideoCaptureFormats formats;
    formats.push_back(format);
    OnSupportedFormats(formats);
  }
}

void MediaStreamRemoteVideoSource::DoRenderFrameOnMainThread(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state() == STARTED)
    DeliverVideoFrame(video_frame);
}

}  // namespace content
