// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

PeerConnectionHandler::PeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client,
    MediaStreamImpl* msi,
    MediaStreamDependencyFactory* dependency_factory)
    : client_(client),
      media_stream_impl_(msi),
      dependency_factory_(dependency_factory),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
}

PeerConnectionHandler::~PeerConnectionHandler() {
}

void PeerConnectionHandler::SetVideoRenderer(
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

void PeerConnectionHandler::initialize(
    const WebKit::WebString& server_configuration,
    const WebKit::WebString& username) {
  native_peer_connection_ = dependency_factory_->CreateRoapPeerConnection(
      UTF16ToUTF8(server_configuration),
      this);
  CHECK(native_peer_connection_);
}

void PeerConnectionHandler::produceInitialOffer(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_add_streams) {
  AddStreams(pending_add_streams);
  native_peer_connection_->CommitStreamChanges();
}

void PeerConnectionHandler::handleInitialOffer(const WebKit::WebString& sdp) {
  native_peer_connection_->ProcessSignalingMessage(UTF16ToUTF8(sdp));
}

void PeerConnectionHandler::processSDP(const WebKit::WebString& sdp) {
  native_peer_connection_->ProcessSignalingMessage(UTF16ToUTF8(sdp));
}

void PeerConnectionHandler::processPendingStreams(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_add_streams,
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
        pending_remove_streams) {
  AddStreams(pending_add_streams);
  RemoveStreams(pending_remove_streams);
  native_peer_connection_->CommitStreamChanges();
}

void PeerConnectionHandler::sendDataStreamMessage(
    const char* data,
    size_t length) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::stop() {
  // TODO(ronghuawu): There's an issue with signaling messages being sent during
  // close. We need to investigate further. Not calling Close() on native
  // PeerConnection is OK for now.
  native_peer_connection_ = NULL;
  media_stream_impl_->ClosePeerConnection();
}

void PeerConnectionHandler::OnError() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::OnMessage(const std::string& msg) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::OnSignalingMessage(const std::string& msg) {
  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnSignalingMessage,
        base::Unretained(this),
        msg));
    return;
  }
  client_->didGenerateSDP(UTF8ToUTF16(msg));
}

void PeerConnectionHandler::OnStateChange(StateType state_changed) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::OnAddStream(webrtc::MediaStreamInterface* stream) {
  if (!stream)
    return;

  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnAddStreamCallback,
        base::Unretained(this),
        stream));
  } else {
    OnAddStreamCallback(stream);
  }
}

void PeerConnectionHandler::OnRemoveStream(
    webrtc::MediaStreamInterface* stream) {
  if (!stream)
    return;

  if (!message_loop_proxy_->BelongsToCurrentThread()) {
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &PeerConnectionHandler::OnRemoveStreamCallback,
        base::Unretained(this),
        stream));
  } else {
    OnRemoveStreamCallback(stream);
  }
}

void PeerConnectionHandler::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::OnIceComplete() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::AddStreams(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>& streams) {
  for (size_t i = 0; i < streams.size(); ++i) {
    talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> stream =
        dependency_factory_->CreateLocalMediaStream(
            UTF16ToUTF8(streams[i].label()));
    WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector;
    streams[i].sources(source_vector);

    // Get and add all tracks.
    for (size_t j = 0; j < source_vector.size(); ++j) {
      webrtc::MediaStreamTrackInterface* track =
          media_stream_impl_->GetLocalMediaStreamTrack(
              UTF16ToUTF8(source_vector[j].id()));
      DCHECK(track);
      if (source_vector[j].type() == WebKit::WebMediaStreamSource::TypeVideo) {
        stream->AddTrack(static_cast<webrtc::VideoTrackInterface*>(track));
      } else {
        stream->AddTrack(static_cast<webrtc::AudioTrackInterface*>(track));
      }
    }

    native_peer_connection_->AddStream(stream);
  }
}

void PeerConnectionHandler::RemoveStreams(
    const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>& streams) {
  talk_base::scoped_refptr<webrtc::StreamCollectionInterface> native_streams =
      native_peer_connection_->local_streams();
  // TODO(perkj): Change libJingle PeerConnection::RemoveStream API to take a
  // label as input instead of stream and return bool.
  for (size_t i = 0; i < streams.size() && native_streams != NULL; ++i) {
    webrtc::LocalMediaStreamInterface* stream =
        static_cast<webrtc::LocalMediaStreamInterface*>(native_streams->find(
            UTF16ToUTF8(streams[i].label())));
    DCHECK(stream);
    native_peer_connection_->RemoveStream(stream);
  }
}

void PeerConnectionHandler::OnAddStreamCallback(
    webrtc::MediaStreamInterface* stream) {
  DCHECK(remote_streams_.find(stream) == remote_streams_.end());
  WebKit::WebMediaStreamDescriptor descriptor =
      CreateWebKitStreamDescriptor(stream);
  remote_streams_.insert(
      std::pair<webrtc::MediaStreamInterface*,
                WebKit::WebMediaStreamDescriptor>(stream, descriptor));
  client_->didAddRemoteStream(descriptor);
}

void PeerConnectionHandler::OnRemoveStreamCallback(
    webrtc::MediaStreamInterface* stream) {
  RemoteStreamMap::iterator it = remote_streams_.find(stream);
  if (it == remote_streams_.end()) {
    NOTREACHED() << "Stream not found";
    return;
  }
  WebKit::WebMediaStreamDescriptor descriptor = it->second;
  DCHECK(!descriptor.isNull());
  remote_streams_.erase(it);
  client_->didRemoveRemoteStream(descriptor);
}

WebKit::WebMediaStreamDescriptor
PeerConnectionHandler::CreateWebKitStreamDescriptor(
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
