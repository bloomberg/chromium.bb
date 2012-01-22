// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

MediaStreamDependencyFactory::MediaStreamDependencyFactory() {}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {}

cricket::WebRtcMediaEngine*
MediaStreamDependencyFactory::CreateWebRtcMediaEngine() {
  return NULL;
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory(
    cricket::MediaEngineInterface* media_engine,
    talk_base::Thread* worker_thread) {
  return false;
}

void MediaStreamDependencyFactory::DeletePeerConnectionFactory() {
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return false;
}

cricket::PortAllocator* MediaStreamDependencyFactory::CreatePortAllocator(
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const webkit_glue::P2PTransport::Config& config) {
  return NULL;
}

webrtc::PeerConnection* MediaStreamDependencyFactory::CreatePeerConnection(
    cricket::PortAllocator* port_allocator,
    talk_base::Thread* signaling_thread) {
  return NULL;
}
