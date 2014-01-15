// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include "content/renderer/media/media_stream_dependency_factory.h"
#include "media/base/video_frame.h"
#include "third_party/libjingle/source/talk/app/webrtc/remotevideocapturer.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"

namespace content {

MediaStreamVideoSource::MediaStreamVideoSource(
    MediaStreamDependencyFactory* factory)
    : factory_(factory),
      width_(0),
      height_(0),
      first_frame_timestamp_(media::kNoTimestamp()) {
  DCHECK(factory_);
}

void MediaStreamVideoSource::AddTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints) {
  factory_->CreateNativeMediaStreamTrack(track);
}

void MediaStreamVideoSource::RemoveTrack(
    const blink::WebMediaStreamTrack& track) {
  // TODO(ronghuawu): What should be done here? Do we really need RemoveTrack?
}

void MediaStreamVideoSource::Init() {
  if (!adapter_) {
    const webrtc::MediaConstraintsInterface* constraints = NULL;
    adapter_ = factory_->CreateVideoSource(new webrtc::RemoteVideoCapturer(),
                                           constraints);
  }
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  // TODO(ronghuawu): Sets WebMediaStreamSource's ready state and notifies the
  // ready state to all registered tracks.
}

void MediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (first_frame_timestamp_ == media::kNoTimestamp()) {
    first_frame_timestamp_ = frame->GetTimestamp();
  }

  cricket::VideoRenderer* input = adapter_->FrameInput();
  if (width_ != frame->coded_size().width() ||
      height_ != frame->coded_size().height()) {
    width_ = frame->coded_size().width();
    height_ = frame->coded_size().height();
    const int reserved = 0;
    input->SetSize(width_, height_, reserved);
  }

  cricket::WebRtcVideoFrame cricket_frame;
  const int64 elapsed_time_ns =
      (frame->GetTimestamp() - first_frame_timestamp_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  const int64 time_stamp_ns = frame->GetTimestamp().InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  const size_t size =
      media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  const size_t pixel_width = 1;
  const size_t pixel_height = 1;
  const int rotation = 0;
  cricket_frame.Alias(frame->data(0), size,
                      width_, height_,
                      pixel_width, pixel_height,
                      elapsed_time_ns, time_stamp_ns,
                      rotation);
  input->RenderFrame(&cricket_frame);
}

MediaStreamVideoSource::~MediaStreamVideoSource() {
}

}  // namespace content
