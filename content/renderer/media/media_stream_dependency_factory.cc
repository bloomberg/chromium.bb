// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include "content/renderer/media/video_capture_module_impl.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "jingle/glue/thread_wrapper.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/libjingle/source/talk/session/phone/dummydevicemanager.h"
#include "third_party/libjingle/source/talk/session/phone/webrtcmediaengine.h"

MediaStreamDependencyFactory::MediaStreamDependencyFactory() {}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {}

cricket::WebRtcMediaEngine*
MediaStreamDependencyFactory::CreateWebRtcMediaEngine() {
  webrtc::AudioDeviceModule* adm = new WebRtcAudioDeviceImpl();
  webrtc::AudioDeviceModule* adm_sc = new WebRtcAudioDeviceImpl();
  return new cricket::WebRtcMediaEngine(adm, adm_sc, NULL);
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory(
    cricket::PortAllocator* port_allocator,
    cricket::MediaEngineInterface* media_engine,
    talk_base::Thread* worker_thread) {
  if (!pc_factory_.get()) {
    scoped_ptr<webrtc::PeerConnectionFactory> factory(
        new webrtc::PeerConnectionFactory(port_allocator,
                                          media_engine,
                                          new cricket::DummyDeviceManager(),
                                          worker_thread));
    if (factory->Initialize())
      pc_factory_.reset(factory.release());
  }
  return pc_factory_.get() != NULL;
}

void MediaStreamDependencyFactory::DeletePeerConnectionFactory() {
  pc_factory_.reset();
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

webrtc::PeerConnection* MediaStreamDependencyFactory::CreatePeerConnection(
    talk_base::Thread* signaling_thread) {
  return pc_factory_->CreatePeerConnection(signaling_thread);
}
