// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

PeerConnectionHandler::PeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client,
    MediaStreamDependencyFactory* dependency_factory)
    : PeerConnectionHandlerBase(dependency_factory),
      client_(client) {
}

PeerConnectionHandler::~PeerConnectionHandler() {
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
  for (size_t i = 0; i < pending_add_streams.size(); ++i)
    AddStream(pending_add_streams[i]);
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
  for (size_t i = 0; i < pending_add_streams.size(); ++i)
    AddStream(pending_add_streams[i]);
  for (size_t i = 0; i < pending_remove_streams.size(); ++i)
    RemoveStream(pending_remove_streams[i]);
  native_peer_connection_->CommitStreamChanges();
}

void PeerConnectionHandler::sendDataStreamMessage(
    const char* data,
    size_t length) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandler::stop() {
  DVLOG(1) << "PeerConnectionHandler::stop";
  // TODO(ronghuawu): There's an issue with signaling messages being sent during
  // close. We need to investigate further. Not calling Close() on native
  // PeerConnection is OK for now.
  native_peer_connection_ = NULL;
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
  // Not used by ROAP PeerConnection.
  NOTREACHED();
}

void PeerConnectionHandler::OnIceComplete() {
  // Not used by ROAP PeerConnection.
  NOTREACHED();
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
