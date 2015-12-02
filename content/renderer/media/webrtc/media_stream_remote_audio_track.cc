// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_audio_track.h"

#include "base/logging.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

MediaStreamRemoteAudioTrack::MediaStreamRemoteAudioTrack(
    const scoped_refptr<webrtc::AudioTrackInterface>& track)
    : MediaStreamTrack(false), track_(track) {
}

MediaStreamRemoteAudioTrack::~MediaStreamRemoteAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
}

void MediaStreamRemoteAudioTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  track_->set_enabled(enabled);
}

void MediaStreamRemoteAudioTrack::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Stop means that a track should be stopped permanently. But
  // since there is no proper way of doing that on a remote track, we can
  // at least disable the track. Blink will not call down to the content layer
  // after a track has been stopped.
  SetEnabled(false);
}

webrtc::AudioTrackInterface* MediaStreamRemoteAudioTrack::GetAudioAdapter() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return track_.get();
}

}  // namespace content
