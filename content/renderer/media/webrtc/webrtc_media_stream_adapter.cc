// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

WebRtcMediaStreamAdapter::WebRtcMediaStreamAdapter(
    const blink::WebMediaStream& web_stream,
    MediaStreamDependencyFactory* factory)
    : web_stream_(web_stream),
      factory_(factory) {
  webrtc_media_stream_ =
      factory_->CreateLocalMediaStream(web_stream.id().utf8());
  CreateAudioTracks();
  CreateVideoTracks();
}

WebRtcMediaStreamAdapter::~WebRtcMediaStreamAdapter() {
}

void WebRtcMediaStreamAdapter::CreateAudioTracks() {
  // A media stream is connected to a peer connection, enable the
  // peer connection mode for the sources.
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    MediaStreamTrack* native_track =
        MediaStreamTrack::GetTrack(audio_tracks[i]);
    if (!native_track || !native_track->is_local_track()) {
      // We don't support connecting remote audio tracks to PeerConnection yet.
      // See issue http://crbug/344303.
      // TODO(xians): Remove this after we support connecting remote audio track
      // to PeerConnection.
      DLOG(ERROR) << "webrtc audio track can not be created with a source "
          "from a remote audio track.";
      NOTIMPLEMENTED();
      continue;
    }

    // This is a local audio track.
    const blink::WebMediaStreamSource& source = audio_tracks[i].source();
    MediaStreamAudioSource* audio_source =
        static_cast<MediaStreamAudioSource*>(source.extraData());
    if (audio_source && audio_source->GetAudioCapturer())
      audio_source->GetAudioCapturer()->EnablePeerConnectionMode();

    webrtc_media_stream_->AddTrack(native_track->GetAudioAdapter());
  }
}

void WebRtcMediaStreamAdapter::CreateVideoTracks() {
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    MediaStreamVideoTrack* native_track =
        MediaStreamVideoTrack::GetVideoTrack(video_tracks[i]);
    webrtc_media_stream_->AddTrack(native_track->GetVideoAdapter());
  }
}

}  // namespace content
