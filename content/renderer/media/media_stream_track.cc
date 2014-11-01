// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_track.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// static
MediaStreamTrack* MediaStreamTrack::GetTrack(
    const blink::WebMediaStreamTrack& track) {
  if (track.isNull())
    return NULL;
  return static_cast<MediaStreamTrack*>(track.extraData());
}

MediaStreamTrack::MediaStreamTrack(bool is_local_track)
    : is_local_track_(is_local_track) {
}

MediaStreamTrack::~MediaStreamTrack() {
}

webrtc::AudioTrackInterface* MediaStreamTrack::GetAudioAdapter() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
