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

webrtc::MediaStreamInterface* PeerConnectionHandlerBase::GetRemoteMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  for (RemoteStreamMap::const_iterator it = remote_streams_.begin();
       it != remote_streams_.end(); ++it) {
    if (it->second.label() == stream.label())
      return it->first;
  }
  return NULL;
}

void PeerConnectionHandlerBase::SetRemoteVideoRenderer(
    const std::string& source_id,
    webrtc::VideoRendererWrapperInterface* renderer) {
  webrtc::VideoTrackInterface* remote_track = FindRemoteVideoTrack(source_id);
  if (remote_track) {
    remote_track->SetRenderer(renderer);
    // TODO(perkj): Remove video_renderers_ from this class when it's not longer
    // needed. See http://code.google.com/p/webrtc/issues/detail?id=447
    video_renderers_.erase(source_id);  // Remove old renderer if exists.
    video_renderers_.insert(std::make_pair(source_id, renderer));
  } else {
    NOTREACHED();
  }
}

void PeerConnectionHandlerBase::AddStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  webrtc::LocalMediaStreamInterface* native_stream =
      media_stream_impl_->GetLocalMediaStream(stream);
  if (native_stream)
    native_peer_connection_->AddStream(native_stream);
  DCHECK(native_stream);
}

void PeerConnectionHandlerBase::RemoveStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  webrtc::LocalMediaStreamInterface* native_stream =
      media_stream_impl_->GetLocalMediaStream(stream);
  if (native_stream)
    native_peer_connection_->RemoveStream(native_stream);
  DCHECK(native_stream);
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

webrtc::VideoTrackInterface* PeerConnectionHandlerBase::FindRemoteVideoTrack(
    const std::string& source_id) {
  talk_base::scoped_refptr<webrtc::StreamCollectionInterface> streams =
      native_peer_connection_->remote_streams();
  for (size_t i = 0; i < streams->count(); ++i) {
    webrtc::VideoTracks* track_list =streams->at(i)->video_tracks();
    for (size_t j =0; j < track_list->count(); ++j) {
      if (track_list->at(j)->label() == source_id) {
        return track_list->at(j);
      }
    }
  }
  return NULL;
}



