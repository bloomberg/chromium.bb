// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

MediaStreamDependencyFactory::MediaStreamDependencyFactory() {}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory) {
  return false;
}

void MediaStreamDependencyFactory::ReleasePeerConnectionFactory() {
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return false;
}

talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const std::string& config,
    webrtc::PeerConnectionObserver* observer) {
  return NULL;
}

talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>
MediaStreamDependencyFactory::CreateLocalMediaStream(const std::string& label) {
  return NULL;
}

talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& label,
    cricket::VideoCapturer* video_device) {
  return NULL;
}

talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface>
MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& label,
    webrtc::AudioDeviceModule* audio_device) {
  return NULL;
}
