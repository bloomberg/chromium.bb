// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "third_party/libjingle/source/talk/session/phone/webrtcmediaengine.h"

MockMediaStreamDependencyFactory::MockMediaStreamDependencyFactory()
    : mock_pc_factory_created_(false) {
}

MockMediaStreamDependencyFactory::~MockMediaStreamDependencyFactory() {}

cricket::WebRtcMediaEngine*
MockMediaStreamDependencyFactory::CreateWebRtcMediaEngine() {
  return new cricket::WebRtcMediaEngine(NULL, NULL, NULL);
}

bool MockMediaStreamDependencyFactory::CreatePeerConnectionFactory(
      cricket::MediaEngineInterface* media_engine,
      talk_base::Thread* worker_thread) {
  mock_pc_factory_created_ = true;
  media_engine_.reset(media_engine);
  return true;
}

void MockMediaStreamDependencyFactory::DeletePeerConnectionFactory() {
  mock_pc_factory_created_ = false;
}

bool MockMediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return mock_pc_factory_created_;
}

cricket::PortAllocator* MockMediaStreamDependencyFactory::CreatePortAllocator(
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const webkit_glue::P2PTransport::Config& config) {
  return NULL;
}

webrtc::PeerConnection* MockMediaStreamDependencyFactory::CreatePeerConnection(
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* signaling_thread) {
  DCHECK(mock_pc_factory_created_);
  return new webrtc::MockPeerConnectionImpl();
}
