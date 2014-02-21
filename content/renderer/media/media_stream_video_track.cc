// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/webrtc/webrtc_video_sink_adapter.h"

namespace content {

// Wrapper which allows to use std::find_if() when adding and removing
// sinks to/from |sinks_|.
struct SinkWrapper {
  explicit SinkWrapper(MediaStreamVideoSink* sink) : sink_(sink) {}
  bool operator()(
      const WebRtcVideoSinkAdapter* owner) {
    return owner->sink() == sink_;
  }
  MediaStreamVideoSink* sink_;
};

MediaStreamVideoTrack::MediaStreamVideoTrack(
    webrtc::VideoTrackInterface* track)
    : MediaStreamTrack(track, false),
      factory_(NULL) {
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamDependencyFactory* factory)
    : MediaStreamTrack(NULL, true),
      factory_(factory) {
  DCHECK(factory_);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(sinks_.empty());
}

void MediaStreamVideoTrack::AddSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find_if(sinks_.begin(), sinks_.end(),
                      SinkWrapper(sink)) == sinks_.end());
  sinks_.push_back(new WebRtcVideoSinkAdapter(GetVideoAdapter(), sink));
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedVector<WebRtcVideoSinkAdapter>::iterator it =
      std::find_if(sinks_.begin(), sinks_.end(), SinkWrapper(sink));
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
}

webrtc::VideoTrackInterface* MediaStreamVideoTrack::GetVideoAdapter() {
  DCHECK_EQ(owner().source().type(), blink::WebMediaStreamSource::TypeVideo);
  if (!track_.get()) {
    MediaStreamVideoSource* source =
          static_cast<MediaStreamVideoSource*>(owner().source().extraData());
    scoped_refptr<webrtc::VideoTrackInterface> video_track(
        factory_->CreateLocalVideoTrack(owner().id().utf8(),
                                        source->GetAdapter()));
    video_track->set_enabled(owner().isEnabled());
    track_ = video_track;
  }
  return static_cast<webrtc::VideoTrackInterface*>(track_.get());
}

}  // namespace content
