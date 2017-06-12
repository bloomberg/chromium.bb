// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include <utility>

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

WebRtcMediaStreamAdapter::WebRtcMediaStreamAdapter(
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    const blink::WebMediaStream& web_stream)
    : factory_(factory),
      track_adapter_map_(track_adapter_map),
      web_stream_(web_stream) {
  webrtc_media_stream_ =
      factory_->CreateLocalMediaStream(web_stream.Id().Utf8());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.AudioTracks(audio_tracks);
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    TrackAdded(audio_track);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.VideoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    TrackAdded(video_track);

  MediaStream* const native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->AddObserver(this);
}

WebRtcMediaStreamAdapter::~WebRtcMediaStreamAdapter() {
  MediaStream* const native_stream = MediaStream::GetMediaStream(web_stream_);
  native_stream->RemoveObserver(this);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.AudioTracks(audio_tracks);
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    TrackRemoved(audio_track);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.VideoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    TrackRemoved(video_track);
}

void WebRtcMediaStreamAdapter::TrackAdded(
    const blink::WebMediaStreamTrack& web_track) {
  std::string track_id = web_track.Id().Utf8();
  DCHECK(adapter_refs_.find(track_id) == adapter_refs_.end());
  bool is_audio_track =
      (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio);
  if (is_audio_track && !MediaStreamAudioTrack::From(web_track)) {
    DLOG(ERROR) << "No native track for blink audio track.";
    return;
  }
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref =
      track_adapter_map_->GetOrCreateLocalTrackAdapter(web_track);
  if (is_audio_track) {
    webrtc_media_stream_->AddTrack(
        static_cast<webrtc::AudioTrackInterface*>(adapter_ref->webrtc_track()));
  } else {
    webrtc_media_stream_->AddTrack(
        static_cast<webrtc::VideoTrackInterface*>(adapter_ref->webrtc_track()));
  }
  adapter_refs_.insert(std::make_pair(track_id, std::move(adapter_ref)));
}

void WebRtcMediaStreamAdapter::TrackRemoved(
    const blink::WebMediaStreamTrack& web_track) {
  std::string track_id = web_track.Id().Utf8();
  auto it = adapter_refs_.find(track_id);
  if (it == adapter_refs_.end()) {
    // This can happen for audio tracks that don't have a source, these would
    // never be added in the first place.
    return;
  }
  if (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio) {
    webrtc_media_stream_->RemoveTrack(
        static_cast<webrtc::AudioTrackInterface*>(it->second->webrtc_track()));
  } else {
    webrtc_media_stream_->RemoveTrack(
        static_cast<webrtc::VideoTrackInterface*>(it->second->webrtc_track()));
  }
  adapter_refs_.erase(it);
}

}  // namespace content
