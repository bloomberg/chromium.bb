// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler_base.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

PeerConnectionHandlerBase::PeerConnectionHandlerBase(
    MediaStreamImpl* msi,
    MediaStreamDependencyFactory* dependency_factory)
    : media_stream_impl_(msi),
      dependency_factory_(dependency_factory),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
}

PeerConnectionHandlerBase::~PeerConnectionHandlerBase() {
}

bool PeerConnectionHandlerBase::HasStream(const std::string& stream_label) {
  webrtc::MediaStreamInterface* stream =
      native_peer_connection_->remote_streams()->find(stream_label);
  return stream != NULL;
}

void PeerConnectionHandlerBase::SetVideoRenderer(
    const std::string& stream_label,
    webrtc::VideoRendererWrapperInterface* renderer) {
  webrtc::MediaStreamInterface* stream =
      native_peer_connection_->remote_streams()->find(stream_label);
  webrtc::VideoTracks* video_tracks = stream->video_tracks();
  // We assume there is only one enabled video track.
  for (size_t i = 0; i < video_tracks->count(); ++i) {
    webrtc::VideoTrackInterface* video_track = video_tracks->at(i);
    if (video_track->enabled()) {
      video_track->SetRenderer(renderer);
      return;
    }
  }
  DVLOG(1) << "No enabled video track.";
}

void PeerConnectionHandlerBase::AddStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> native_stream =
      dependency_factory_->CreateLocalMediaStream(UTF16ToUTF8(stream.label()));
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector;
  stream.sources(source_vector);

  // Get and add all tracks.
  for (size_t i = 0; i < source_vector.size(); ++i) {
    webrtc::MediaStreamTrackInterface* track = media_stream_impl_
        ->GetLocalMediaStreamTrack(UTF16ToUTF8(source_vector[i].id()));
    DCHECK(track);
    if (source_vector[i].type() == WebKit::WebMediaStreamSource::TypeVideo) {
      native_stream->AddTrack(static_cast<webrtc::VideoTrackInterface*>(track));
    } else {
      DCHECK(source_vector[i].type() ==
          WebKit::WebMediaStreamSource::TypeAudio);
      native_stream->AddTrack(static_cast<webrtc::AudioTrackInterface*>(track));
    }
  }

  native_peer_connection_->AddStream(native_stream);
}

void PeerConnectionHandlerBase::RemoveStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  talk_base::scoped_refptr<webrtc::StreamCollectionInterface> native_streams =
      native_peer_connection_->local_streams();
  if (!native_streams)
    return;
  // TODO(perkj): Change libJingle PeerConnection::RemoveStream API to take a
  // label as input instead of stream and return bool.
  webrtc::LocalMediaStreamInterface* native_stream =
      static_cast<webrtc::LocalMediaStreamInterface*>(native_streams->find(
          UTF16ToUTF8(stream.label())));
  DCHECK(native_stream);
  native_peer_connection_->RemoveStream(native_stream);
}

WebKit::WebMediaStreamDescriptor
PeerConnectionHandlerBase::CreateWebKitStreamDescriptor(
    webrtc::MediaStreamInterface* stream) {
  webrtc::AudioTracks* audio_tracks = stream->audio_tracks();
  webrtc::VideoTracks* video_tracks = stream->video_tracks();
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(
      audio_tracks->count() + video_tracks->count());

  // Add audio tracks.
  size_t i = 0;
  for (; i < audio_tracks->count(); ++i) {
    webrtc::AudioTrackInterface* audio_track = audio_tracks->at(i);
    DCHECK(audio_track);
    source_vector[i].initialize(
        // TODO(grunell): Set id to something unique.
        UTF8ToUTF16(audio_track->label()),
        WebKit::WebMediaStreamSource::TypeAudio,
        UTF8ToUTF16(audio_track->label()));
  }

  // Add video tracks.
  for (i = 0; i < video_tracks->count(); ++i) {
    webrtc::VideoTrackInterface* video_track = video_tracks->at(i);
    DCHECK(video_track);
    source_vector[audio_tracks->count() + i].initialize(
        // TODO(grunell): Set id to something unique.
        UTF8ToUTF16(video_track->label()),
        WebKit::WebMediaStreamSource::TypeVideo,
        UTF8ToUTF16(video_track->label()));
  }

  WebKit::WebMediaStreamDescriptor descriptor;
  descriptor.initialize(UTF8ToUTF16(stream->label()), source_vector);

  return descriptor;
}
