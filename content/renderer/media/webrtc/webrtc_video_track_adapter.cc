// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_track_adapter.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"

namespace {

bool ConstraintKeyExists(const blink::WebMediaConstraints& constraints,
                         const blink::WebString& name) {
  blink::WebString value_str;
  return constraints.getMandatoryConstraintValue(name, value_str) ||
      constraints.getOptionalConstraintValue(name, value_str);
}

}  // anonymouse namespace

namespace content {

WebRtcVideoTrackAdapter::WebRtcVideoTrackAdapter(
    const blink::WebMediaStreamTrack& track,
    MediaStreamDependencyFactory* factory)
    : web_track_(track) {
  MediaStreamVideoSink::AddToVideoTrack(this, web_track_);

  const blink::WebMediaConstraints& constraints =
      MediaStreamVideoTrack::GetVideoTrack(track)->constraints();

  bool is_screencast = ConstraintKeyExists(
      constraints, base::UTF8ToUTF16(kMediaStreamSource));
  capture_adapter_ = factory->CreateVideoCapturer(is_screencast);

  // |video_source| owns |capture_adapter|
  video_source_ = factory->CreateVideoSource(capture_adapter_,
                                             track.source().constraints());

  video_track_ = factory->CreateLocalVideoTrack(web_track_.id().utf8(),
                                                video_source_);
  video_track_->set_enabled(web_track_.isEnabled());

  DVLOG(3) << "WebRtcVideoTrackAdapter ctor() : is_screencast "
           << is_screencast;
}

WebRtcVideoTrackAdapter::~WebRtcVideoTrackAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "WebRtcVideoTrackAdapter dtor().";
  MediaStreamVideoSink::RemoveFromVideoTrack(this, web_track_);
}

void WebRtcVideoTrackAdapter::OnEnabledChanged(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_track_->set_enabled(enabled);
}

void WebRtcVideoTrackAdapter::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_adapter_->OnFrameCaptured(frame);
}

}  // namespace content

