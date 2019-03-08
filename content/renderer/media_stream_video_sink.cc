// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_stream_video_sink.h"

#include "content/public/renderer/media_stream_utils.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"

namespace content {

MediaStreamVideoSink::MediaStreamVideoSink() : blink::WebMediaStreamSink() {}

MediaStreamVideoSink::~MediaStreamVideoSink() {
  // Ensure this sink has disconnected from the track.
  DisconnectFromTrack();
}

void MediaStreamVideoSink::ConnectToTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::VideoCaptureDeliverFrameCB& callback,
    bool is_sink_secure) {
  DCHECK(connected_track_.IsNull());
  connected_track_ = track;
  AddSinkToMediaStreamTrack(track, this, callback, is_sink_secure);
}

void MediaStreamVideoSink::DisconnectFromTrack() {
  RemoveSinkFromMediaStreamTrack(connected_track_, this);
  connected_track_.Reset();
}

void MediaStreamVideoSink::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  if (connected_track_.IsNull())
    return;
  OnFrameDroppedAtMediaStreamSink(connected_track_, reason);
}

}  // namespace content
