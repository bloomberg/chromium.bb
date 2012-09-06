// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include <vector>

#include "content/renderer/media/rtc_video_capturer.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "jingle/glue/thread_wrapper.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

class P2PPortAllocatorFactory : public webrtc::PortAllocatorFactoryInterface {
 public:
  P2PPortAllocatorFactory(
      content::P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory)
      : socket_dispatcher_(socket_dispatcher),
        network_manager_(network_manager),
        socket_factory_(socket_factory) {
  }

  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) OVERRIDE {
    WebKit::WebFrame* web_frame = WebKit::WebFrame::frameForCurrentContext();
    if (!web_frame) {
      LOG(ERROR) << "WebFrame is NULL.";
      return NULL;
    }
    content::P2PPortAllocator::Config config;
    if (stun_servers.size() > 0) {
      config.stun_server = stun_servers[0].server.hostname();
      config.stun_server_port = stun_servers[0].server.port();
    }
    if (turn_configurations.size() > 0) {
      config.relay_server = turn_configurations[0].server.hostname();
      config.relay_server_port = turn_configurations[0].server.port();
      config.relay_username = turn_configurations[0].username;
      config.relay_password = turn_configurations[0].password;
    }

    return new content::P2PPortAllocator(web_frame,
                                         socket_dispatcher_,
                                         network_manager_,
                                         socket_factory_,
                                         config);
  }

 protected:
  virtual ~P2PPortAllocatorFactory() {}

 private:
  // socket_dispatcher_ is a weak reference, owned by RenderView. It's valid
  // for the lifetime of RenderView.
  content::P2PSocketDispatcher* socket_dispatcher_;
  // network_manager_ and socket_factory_ are a weak references, owned by
  // MediaStreamImpl.
  talk_base::NetworkManager* network_manager_;
  talk_base::PacketSocketFactory* socket_factory_;
};

MediaStreamDependencyFactory::MediaStreamDependencyFactory(
    VideoCaptureImplManager* vc_manager)
    : vc_manager_(vc_manager) {
}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory) {
  if (!pc_factory_.get()) {
    talk_base::scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            socket_dispatcher,
            network_manager,
            socket_factory);

    DCHECK(!audio_device_);
    audio_device_ = new WebRtcAudioDeviceImpl();
    talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
        webrtc::CreatePeerConnectionFactory(worker_thread,
                                            signaling_thread,
                                            pa_factory.release(),
                                            audio_device_));
    if (factory.get())
      pc_factory_ = factory.release();
  }
  return pc_factory_.get() != NULL;
}

void MediaStreamDependencyFactory::ReleasePeerConnectionFactory() {
  if (pc_factory_.get())
    pc_factory_ = NULL;
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const std::string& config,
    webrtc::PeerConnectionObserver* observer) {
  return pc_factory_->CreatePeerConnection(config, observer);
}

talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>
MediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return pc_factory_->CreateLocalMediaStream(label);
}

talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& label,
    int video_session_id) {
  RtcVideoCapturer* capturer = new RtcVideoCapturer(video_session_id,
                                                    vc_manager_.get());

  // The video track takes ownership of |capturer|.
  return pc_factory_->CreateLocalVideoTrack(label,
                                            capturer);
}

talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface>
MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& label,
    webrtc::AudioDeviceModule* audio_device) {
  return pc_factory_->CreateLocalAudioTrack(label, audio_device);
}

webrtc::SessionDescriptionInterface*
MediaStreamDependencyFactory::CreateSessionDescription(const std::string& sdp) {
  return webrtc::CreateSessionDescription(sdp);
}

webrtc::IceCandidateInterface* MediaStreamDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

void MediaStreamDependencyFactory::SetAudioDeviceSessionId(int session_id) {
  audio_device_->SetSessionId(session_id);
}
