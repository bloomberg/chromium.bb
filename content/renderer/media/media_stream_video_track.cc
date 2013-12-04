// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

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

MediaStreamVideoTrack::MediaStreamVideoTrack(webrtc::VideoTrackInterface* track,
                                             bool is_local_track)
    : MediaStreamTrackExtraData(track, is_local_track),
      video_track_(track) {
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(sinks_.empty());
}

void MediaStreamVideoTrack::AddSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(std::find_if(sinks_.begin(), sinks_.end(),
                      SinkWrapper(sink)) == sinks_.end());
  sinks_.push_back(new WebRtcVideoSinkAdapter(video_track_, sink));
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScopedVector<WebRtcVideoSinkAdapter>::iterator it =
      std::find_if(sinks_.begin(), sinks_.end(), SinkWrapper(sink));
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
}

}  // namespace content
