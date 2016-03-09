// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_source.h"

#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

namespace content {

MediaStreamAudioSource::MediaStreamAudioSource(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const SourceStoppedCallback& stop_callback,
    PeerConnectionDependencyFactory* factory)
    : render_frame_id_(render_frame_id), factory_(factory),
      weak_factory_(this) {
  SetDeviceInfo(device_info);
  SetStopCallback(stop_callback);
}

MediaStreamAudioSource::MediaStreamAudioSource()
    : render_frame_id_(-1), factory_(NULL), weak_factory_(this) {
}

MediaStreamAudioSource::~MediaStreamAudioSource() {}

// static
MediaStreamAudioSource* MediaStreamAudioSource::From(
    const blink::WebMediaStreamSource& source) {
  if (source.isNull() ||
      source.getType() != blink::WebMediaStreamSource::TypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioSource*>(source.getExtraData());
}

void MediaStreamAudioSource::DoStopSource() {
  if (audio_capturer_)
    audio_capturer_->Stop();
  if (webaudio_capturer_)
    webaudio_capturer_->Stop();
}

void MediaStreamAudioSource::AddTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  // TODO(xians): Properly implement for audio sources.
  if (!local_audio_source_.get()) {
    if (!factory_->InitializeMediaStreamAudioSource(render_frame_id_,
                                                    constraints, this)) {
      // The source failed to start.
      // UserMediaClientImpl rely on the |stop_callback| to be triggered when
      // the last track is removed from the source. But in this case, the
      // source is is not even started. So we need to fail both adding the
      // track and trigger |stop_callback|.
      callback.Run(this, MEDIA_DEVICE_TRACK_START_FAILURE, "");
      StopSource();
      return;
    }
  }

  factory_->CreateLocalAudioTrack(track);
  callback.Run(this, MEDIA_DEVICE_OK, "");
}

void MediaStreamAudioSource::StopAudioDeliveryTo(MediaStreamAudioTrack* track) {
  DCHECK(track);
  if (audio_capturer_) {
    // The cast here is safe because only WebRtcLocalAudioTracks are ever used
    // with WebRtcAudioCapturer sources.
    //
    // TODO(miu): That said, this ugly cast won't be necessary after my
    // soon-upcoming refactoring change.
    audio_capturer_->RemoveTrack(static_cast<WebRtcLocalAudioTrack*>(track));
  }
  if (webaudio_capturer_) {
    // A separate source is created for each track, so just stop the source.
    webaudio_capturer_->Stop();
  }
}

}  // namespace content
