// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

#include "content/renderer/media/media_stream_dependency_factory.h"

namespace content {

//static
blink::WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled,
    MediaStreamDependencyFactory* factory) {
  blink::WebMediaStreamTrack track;
  track.initialize(source->owner());
  track.setExtraData(new MediaStreamVideoTrack(source,
                                               constraints,
                                               callback,
                                               enabled,
                                               factory));
  return track;
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::GetVideoTrack(
     const blink::WebMediaStreamTrack& track) {
  return static_cast<MediaStreamVideoTrack*>(track.extraData());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled,
    MediaStreamDependencyFactory* factory)
    : MediaStreamTrack(NULL, true),
      enabled_(enabled),
      source_(source),
      factory_(factory) {
  source->AddTrack(this, constraints, callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(sinks_.empty());
  source_->RemoveTrack(this);
}

void MediaStreamVideoTrack::AddSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<MediaStreamVideoSink*>::iterator it =
      std::find(sinks_.begin(), sinks_.end(), sink);
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
}

webrtc::VideoTrackInterface* MediaStreamVideoTrack::GetVideoAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
  MediaStreamTrack::SetEnabled(enabled);
  for (std::vector<MediaStreamVideoSink*>::iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnEnabledChanged(enabled);
  }
}

void MediaStreamVideoTrack::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_)
    return;

  for (std::vector<MediaStreamVideoSink*>::iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnVideoFrame(frame);
  }
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (std::vector<MediaStreamVideoSink*>::iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnReadyStateChanged(state);
  }
}

}  // namespace content
