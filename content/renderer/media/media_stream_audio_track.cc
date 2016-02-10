// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_track.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

MediaStreamAudioTrack::MediaStreamAudioTrack(bool is_local_track)
    : MediaStreamTrack(is_local_track) {
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
}

// static
MediaStreamAudioTrack* MediaStreamAudioTrack::GetTrack(
    const blink::WebMediaStreamTrack& track) {
  if (track.isNull() ||
      track.source().type() != blink::WebMediaStreamSource::TypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioTrack*>(track.extraData());
}

webrtc::AudioTrackInterface* MediaStreamAudioTrack::GetAudioAdapter() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
