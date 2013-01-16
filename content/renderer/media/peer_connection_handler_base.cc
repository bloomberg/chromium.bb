// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler_base.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace content {

// TODO(hta): Unify implementations of these functions from MediaStreamCenter
static webrtc::LocalMediaStreamInterface* GetLocalNativeMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  if (extra_data)
    return extra_data->local_stream();
  return NULL;
}

template <class TrackList>
static webrtc::MediaStreamTrackInterface* GetLocalTrack(
    const std::string& source_id,
    TrackList* tracks) {
  for (size_t i = 0; i < tracks->count(); ++i) {
    if (tracks->at(i)->id() == source_id)
      return tracks->at(i);
  }
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
    const WebKit::WebMediaStreamDescriptor& stream,
    const webrtc::MediaConstraintsInterface* constraints) {
  webrtc::LocalMediaStreamInterface* native_stream =
      GetLocalNativeMediaStream(stream);
  if (!native_stream)
    return false;
  return native_peer_connection_->AddStream(native_stream, constraints);
}

void PeerConnectionHandlerBase::RemoveStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  webrtc::LocalMediaStreamInterface* native_stream =
      GetLocalNativeMediaStream(stream);
  if (native_stream)
    native_peer_connection_->RemoveStream(native_stream);
  DCHECK(native_stream);
}

WebKit::WebMediaStreamDescriptor
PeerConnectionHandlerBase::CreateWebKitStreamDescriptor(
    webrtc::MediaStreamInterface* stream) {
  webrtc::AudioTracks* audio_tracks = stream->audio_tracks();
  webrtc::VideoTracks* video_tracks = stream->video_tracks();
  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_source_vector(
      audio_tracks->count());
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_source_vector(
      video_tracks->count());

  // Add audio tracks.
  size_t i = 0;
  for (; i < audio_tracks->count(); ++i) {
    webrtc::AudioTrackInterface* audio_track = audio_tracks->at(i);
    DCHECK(audio_track);
    audio_source_vector[i].initialize(
        UTF8ToUTF16(audio_track->id()),
        WebKit::WebMediaStreamSource::TypeAudio,
        UTF8ToUTF16(audio_track->id()));
  }

  // Add video tracks.
  for (i = 0; i < video_tracks->count(); ++i) {
    webrtc::VideoTrackInterface* video_track = video_tracks->at(i);
    DCHECK(video_track);
    video_source_vector[i].initialize(
        UTF8ToUTF16(video_track->id()),
        WebKit::WebMediaStreamSource::TypeVideo,
        UTF8ToUTF16(video_track->id()));
  }
  WebKit::WebMediaStreamDescriptor descriptor;
  descriptor.initialize(UTF8ToUTF16(stream->label()),
                        audio_source_vector, video_source_vector);
  descriptor.setExtraData(new MediaStreamExtraData(stream));
  return descriptor;
}

webrtc::MediaStreamTrackInterface*
PeerConnectionHandlerBase::GetLocalNativeMediaStreamTrack(
      const WebKit::WebMediaStreamDescriptor& stream,
      const WebKit::WebMediaStreamComponent& component) {
  std::string source_id = UTF16ToUTF8(component.source().id());
  webrtc::MediaStreamInterface* native_stream
      = GetLocalNativeMediaStream(stream);
  if (!native_stream) {
    return NULL;
  }
  if (component.source().type() == WebKit::WebMediaStreamSource::TypeAudio) {
    return GetLocalTrack<webrtc::AudioTracks>(
        source_id, native_stream->audio_tracks());
  }
  if (component.source().type() == WebKit::WebMediaStreamSource::TypeVideo) {
    return GetLocalTrack<webrtc::VideoTracks>(
        source_id, native_stream->video_tracks());
  }
  NOTIMPLEMENTED();  // We have an unknown type of media stream track.
  return NULL;
}

}  // namespace content
