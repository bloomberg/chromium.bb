// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      cricket::PortAllocator* port_allocator,
      cricket::MediaEngineInterface* media_engine,
      talk_base::Thread* worker_thread) {
  mock_pc_factory_created_ = true;
  return true;
}

void MockMediaStreamDependencyFactory::DeletePeerConnectionFactory() {
  mock_pc_factory_created_ = false;
}

bool MockMediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return mock_pc_factory_created_;
}

webrtc::PeerConnection* MockMediaStreamDependencyFactory::CreatePeerConnection(
    talk_base::Thread* signaling_thread) {
  DCHECK(mock_pc_factory_created_);
  return new webrtc::MockPeerConnectionImpl();
}
