// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

namespace content {

//static
blink::WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  blink::WebMediaStreamTrack track;
  track.initialize(source->owner());
  track.setExtraData(new MediaStreamVideoTrack(source,
                                               constraints,
                                               callback,
                                               enabled));
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
    bool enabled)
    : MediaStreamTrack(NULL, true),
      enabled_(enabled),
      constraints_(constraints),
      source_(source) {
  source->AddTrack(this, constraints, callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(sinks_.empty());
  Stop();
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

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
  MediaStreamTrack::SetEnabled(enabled);
  for (std::vector<MediaStreamVideoSink*>::iterator it = sinks_.begin();
       it != sinks_.end(); ++it) {
    (*it)->OnEnabledChanged(enabled);
  }
}

void MediaStreamVideoTrack::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source_) {
    source_->RemoveTrack(this);
    source_ = NULL;
  }
  OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateEnded);
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
