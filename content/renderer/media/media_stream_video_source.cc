// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "media/base/video_frame.h"
#include "third_party/libjingle/source/talk/app/webrtc/remotevideocapturer.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"

namespace content {

MediaStreamVideoSource::MediaStreamVideoSource(
    MediaStreamDependencyFactory* factory)
    : initializing_(false),
      factory_(factory),
      width_(0),
      height_(0),
      first_frame_timestamp_(media::kNoTimestamp()) {
  DCHECK(factory_);
}

MediaStreamVideoSource::~MediaStreamVideoSource() {
  if (initializing_) {
    adapter_->UnregisterObserver(this);
  }
}

void MediaStreamVideoSource::AddTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  if (!adapter_) {
    // Create the webrtc::MediaStreamVideoSourceInterface adapter.
    InitAdapter(constraints);
    DCHECK(adapter_);

    current_constraints_ = constraints;
    initializing_ = true;
    // Register to the adapter to get notified when it has been started
    // successfully.
    adapter_->RegisterObserver(this);
  }

  // TODO(perkj): Currently, reconfiguring the source is not supported. For now
  // we ignore if |constraints| do not match the constraints that was used
  // when the source was started

  // There might be multiple tracks attaching to the source while it is being
  // configured.
  constraints_callbacks_.push_back(callback);
  TriggerConstraintsCallbackOnStateChange();

  // TODO(perkj): Use the MediaStreamDependencyFactory for now to create the
  // MediaStreamVideoTrack since creation is currently still depending on
  // libjingle. The webrtc video track implementation will attach to the
  // webrtc::VideoSourceInterface returned by GetAdapter() to receive video
  // frames.
  factory_->CreateNativeMediaStreamTrack(track);
}

void MediaStreamVideoSource::RemoveTrack(
    const blink::WebMediaStreamTrack& track) {
  // TODO(ronghuawu): What should be done here? Do we really need RemoveTrack?
}

void MediaStreamVideoSource::InitAdapter(
    const blink::WebMediaConstraints& constraints) {
  DCHECK(!adapter_);
  RTCMediaConstraints webrtc_constraints(constraints);
  adapter_ = factory_->CreateVideoSource(new webrtc::RemoteVideoCapturer(),
                                         &webrtc_constraints);
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

void MediaStreamVideoSource::OnChanged() {
  DCHECK(CalledOnValidThread());
  TriggerConstraintsCallbackOnStateChange();
}

void MediaStreamVideoSource::TriggerConstraintsCallbackOnStateChange() {
  if (adapter_->state() == webrtc::MediaSourceInterface::kInitializing)
    return;

  if (initializing_) {
    adapter_->UnregisterObserver(this);
    initializing_ = false;
  }

  std::vector<ConstraintsCallback> callbacks;
  callbacks.swap(constraints_callbacks_);

  bool success = (adapter_->state() == webrtc::MediaSourceInterface::kLive);
  for (std::vector<ConstraintsCallback>::iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    if (!it->is_null())
      it->Run(this, success);
  }
}

}  // namespace content
