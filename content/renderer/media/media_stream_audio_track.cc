// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_track.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

MediaStreamAudioTrack::MediaStreamAudioTrack(bool is_local_track)
    : MediaStreamTrack(is_local_track) {
  DVLOG(1) << "MediaStreamAudioTrack::MediaStreamAudioTrack(is a "
           << (is_local_track ? "local" : "remote") << " track)";
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamAudioTrack::~MediaStreamAudioTrack()";
  DCHECK(stop_callback_.is_null())
      << "BUG: Subclass must ensure Stop() is called.";
}

// static
MediaStreamAudioTrack* MediaStreamAudioTrack::From(
    const blink::WebMediaStreamTrack& track) {
  if (track.isNull() ||
      track.source().getType() != blink::WebMediaStreamSource::TypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioTrack*>(track.getExtraData());
}

void MediaStreamAudioTrack::Start(const base::Closure& stop_callback) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!stop_callback.is_null());
  DCHECK(stop_callback_.is_null());
  DVLOG(1) << "MediaStreamAudioTrack::Start()";
  stop_callback_ = stop_callback;
}

void MediaStreamAudioTrack::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "MediaStreamAudioTrack::Stop()";
  if (!stop_callback_.is_null())
    base::ResetAndReturn(&stop_callback_).Run();
  OnStop();
}

void MediaStreamAudioTrack::OnStop() {}

webrtc::AudioTrackInterface* MediaStreamAudioTrack::GetAudioAdapter() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
