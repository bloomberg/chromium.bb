// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_audio_sink.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_track_extra_data.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

void MediaStreamAudioSink::AddToAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  DCHECK(track.source().type() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamTrackExtraData* extra_data =
      static_cast<MediaStreamTrackExtraData*>(track.extraData());
  // TODO(xians): Support remote audio track.
  DCHECK(extra_data->is_local_track());
  WebRtcLocalAudioTrack* audio_track =
      static_cast<WebRtcLocalAudioTrack*>(extra_data->track().get());
  audio_track->AddSink(sink);
}

void MediaStreamAudioSink::RemoveFromAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrackExtraData* extra_data =
      static_cast<MediaStreamTrackExtraData*>(track.extraData());
  // TODO(xians): Support remote audio track.
  DCHECK(extra_data->is_local_track());
  WebRtcLocalAudioTrack* audio_track =
      static_cast<WebRtcLocalAudioTrack*>(extra_data->track().get());
  audio_track->RemoveSink(sink);
}

}  // namespace content
