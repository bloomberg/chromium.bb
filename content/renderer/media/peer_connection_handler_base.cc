// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler_base.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace content {

// TODO(hta): Unify implementations of these functions from MediaStreamCenter
static webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& stream) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  if (extra_data)
    return extra_data->stream();
  return NULL;
}

PeerConnectionHandlerBase::PeerConnectionHandlerBase(
    MediaStreamDependencyFactory* dependency_factory)
    : dependency_factory_(dependency_factory),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
}

PeerConnectionHandlerBase::~PeerConnectionHandlerBase() {
}

bool PeerConnectionHandlerBase::AddStream(
    const WebKit::WebMediaStream& stream,
    const webrtc::MediaConstraintsInterface* constraints) {
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  if (!native_stream)
    return false;
  return native_peer_connection_->AddStream(native_stream, constraints);
}

void PeerConnectionHandlerBase::RemoveStream(
    const WebKit::WebMediaStream& stream) {
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  if (native_stream)
    native_peer_connection_->RemoveStream(native_stream);
  DCHECK(native_stream);
}

WebKit::WebMediaStream
PeerConnectionHandlerBase::CreateRemoteWebKitMediaStream(
    webrtc::MediaStreamInterface* stream) {
  webrtc::AudioTrackVector audio_tracks = stream->GetAudioTracks();
  webrtc::VideoTrackVector video_tracks = stream->GetVideoTracks();
  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_source_vector(
      audio_tracks.size());
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_source_vector(
      video_tracks.size());

  // Add audio tracks.
  size_t i = 0;
  for (; i < audio_tracks.size(); ++i) {
    webrtc::AudioTrackInterface* audio_track = audio_tracks[i];
    DCHECK(audio_track);
    audio_source_vector[i].initialize(
        UTF8ToUTF16(audio_track->id()),
        WebKit::WebMediaStreamSource::TypeAudio,
        UTF8ToUTF16(audio_track->id()));
  }

  // Add video tracks.
  for (i = 0; i < video_tracks.size(); ++i) {
    webrtc::VideoTrackInterface* video_track = video_tracks[i];
    DCHECK(video_track);
    video_source_vector[i].initialize(
        UTF8ToUTF16(video_track->id()),
        WebKit::WebMediaStreamSource::TypeVideo,
        UTF8ToUTF16(video_track->id()));
  }
  WebKit::WebMediaStream descriptor;
  descriptor.initialize(UTF8ToUTF16(stream->label()),
                        audio_source_vector, video_source_vector);
  descriptor.setExtraData(new MediaStreamExtraData(stream, false));
  return descriptor;
}

webrtc::MediaStreamTrackInterface*
PeerConnectionHandlerBase::GetLocalNativeMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& track) {
  std::string track_id = UTF16ToUTF8(track.id());
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  if (!native_stream) {
    return NULL;
  }
  if (track.source().type() == WebKit::WebMediaStreamSource::TypeAudio) {
    return native_stream->FindAudioTrack(track_id);
  }
  if (track.source().type() == WebKit::WebMediaStreamSource::TypeVideo) {
    return native_stream->FindVideoTrack(track_id);
  }
  NOTIMPLEMENTED();  // We have an unknown type of media stream track.
  return NULL;
}

}  // namespace content
