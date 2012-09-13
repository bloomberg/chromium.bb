// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_capturer.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "jingle/glue/thread_wrapper.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"

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
  scoped_refptr<content::P2PSocketDispatcher> socket_dispatcher_;
  // |network_manager_| and |socket_factory_| are a weak references, owned by
  // MediaStreamDependencyFactory.
  talk_base::NetworkManager* network_manager_;
  talk_base::PacketSocketFactory* socket_factory_;
};

MediaStreamDependencyFactory::MediaStreamDependencyFactory(
    VideoCaptureImplManager* vc_manager,
    content::P2PSocketDispatcher* p2p_socket_dispatcher)
    : network_manager_(NULL),
      vc_manager_(vc_manager),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread") {
}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {
  CleanupPeerConnectionFactory();
}

WebKit::WebPeerConnection00Handler*
MediaStreamDependencyFactory::CreatePeerConnectionHandlerJsep(
    WebKit::WebPeerConnection00HandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitPeerConnection00.
  UpdateWebRTCMethodCount(WEBKIT_PEER_CONNECTION);

  if (!EnsurePeerConnectionFactory())
    return NULL;

  return new PeerConnectionHandlerJsep(client, this);
}

WebKit::WebRTCPeerConnectionHandler*
MediaStreamDependencyFactory::CreateRTCPeerConnectionHandler(
    WebKit::WebRTCPeerConnectionHandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitRTCPeerConnection.
  UpdateWebRTCMethodCount(WEBKIT_RTC_PEER_CONNECTION);

  if (!EnsurePeerConnectionFactory())
    return NULL;

  return new RTCPeerConnectionHandler(client, this);
}

bool MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    WebKit::WebMediaStreamDescriptor* description) {
  // Creating the peer connection factory can fail if for example the audio
  // (input or output) or video device cannot be opened. Handling such cases
  // better is a higher level design discussion which involves the media
  // manager, webrtc and libjingle. We cannot create any native
  // track objects however, so we'll just have to skip that. Furthermore,
  // creating a peer connection later on will fail if we don't have a factory.
  if (!EnsurePeerConnectionFactory())
    return false;

  std::string label = UTF16ToUTF8(description->label());
  scoped_refptr<webrtc::LocalMediaStreamInterface> native_stream =
      CreateLocalMediaStream(label);

  // Add audio tracks.
  WebKit::WebVector<WebKit::WebMediaStreamComponent> audio_components;
  description->audioSources(audio_components);
  for (size_t i = 0; i < audio_components.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = audio_components[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }
    // TODO(perkj): Refactor the creation of audio tracks to use a proper
    // interface for receiving audio input data. Currently NULL is passed since
    // the |audio_device| is the wrong class and is unused.
    scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
        CreateLocalAudioTrack(UTF16ToUTF8(source.id()), NULL));
    native_stream->AddTrack(audio_track);
    audio_track->set_enabled(audio_components[i].isEnabled());
    // TODO(xians): This set the source of all audio tracks to the same
    // microphone. Implement support for setting the source per audio track
    // instead.
    SetAudioDeviceSessionId(source_data->device_info().session_id);
  }

  // Add video tracks.
  WebKit::WebVector<WebKit::WebMediaStreamComponent> video_components;
  description->videoSources(video_components);
  for (size_t i = 0; i < video_components.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = video_components[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }
    scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
        CreateLocalVideoTrack(UTF16ToUTF8(source.id()),
                              source_data->device_info().session_id));
    native_stream->AddTrack(video_track);
    video_track->set_enabled(video_components[i].isEnabled());
  }

  description->setExtraData(new MediaStreamExtraData(native_stream));
  return true;
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory) {
  if (!pc_factory_.get()) {
    scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            socket_dispatcher,
            network_manager,
            socket_factory);

    DCHECK(!audio_device_);
    audio_device_ = new WebRtcAudioDeviceImpl();
    scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
        webrtc::CreatePeerConnectionFactory(worker_thread,
                                            signaling_thread,
                                            pa_factory.release(),
                                            audio_device_));
    if (factory.get())
      pc_factory_ = factory.release();
  }
  return pc_factory_.get() != NULL;
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const std::string& config,
    webrtc::PeerConnectionObserver* observer) {
  return pc_factory_->CreatePeerConnection(config, observer).get();
}

scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const webrtc::JsepInterface::IceServers& ice_servers,
    const webrtc::MediaConstraintsInterface* constraints,
    webrtc::PeerConnectionObserver* observer) {
  return pc_factory_->CreatePeerConnection(
      ice_servers, constraints, observer).get();
}

scoped_refptr<webrtc::LocalMediaStreamInterface>
MediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return pc_factory_->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::LocalVideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& label,
    int video_session_id) {
  RtcVideoCapturer* capturer = new RtcVideoCapturer(video_session_id,
                                                    vc_manager_.get());

  // The video track takes ownership of |capturer|.
  return pc_factory_->CreateLocalVideoTrack(label, capturer).get();
}

scoped_refptr<webrtc::LocalAudioTrackInterface>
MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& label,
    webrtc::AudioDeviceModule* audio_device) {
  return pc_factory_->CreateLocalAudioTrack(label, audio_device).get();
}

webrtc::SessionDescriptionInterface*
MediaStreamDependencyFactory::CreateSessionDescription(const std::string& sdp) {
  return webrtc::CreateSessionDescription(sdp);
}

webrtc::SessionDescriptionInterface*
MediaStreamDependencyFactory::CreateSessionDescription(const std::string& type,
                                                       const std::string& sdp) {
  return webrtc::CreateSessionDescription(type, sdp);
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

void MediaStreamDependencyFactory::InitializeWorkerThread(
    talk_base::Thread** thread,
    base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}

void MediaStreamDependencyFactory::CreateIpcNetworkManagerOnWorkerThread(
    base::WaitableEvent* event) {
  DCHECK_EQ(MessageLoop::current(), chrome_worker_thread_.message_loop());
  network_manager_ = new content::IpcNetworkManager(p2p_socket_dispatcher_);
  event->Signal();
}

void MediaStreamDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK_EQ(MessageLoop::current(), chrome_worker_thread_.message_loop());
  delete network_manager_;
  network_manager_ = NULL;
}

bool MediaStreamDependencyFactory::EnsurePeerConnectionFactory() {
  DCHECK(CalledOnValidThread());
  if(PeerConnectionFactoryCreated())
    return true;

  if (!signaling_thread_) {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
    signaling_thread_ = jingle_glue::JingleThreadWrapper::current();
    CHECK(signaling_thread_);
  }

  if (!worker_thread_) {
    if (!chrome_worker_thread_.IsRunning()) {
      if (!chrome_worker_thread_.Start()) {
        LOG(ERROR) << "Could not start worker thread";
        signaling_thread_ = NULL;
        return false;
      }
    }
    base::WaitableEvent event(true, false);
    chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &MediaStreamDependencyFactory::InitializeWorkerThread,
        base::Unretained(this),
        &worker_thread_,
        &event));
    event.Wait();
    DCHECK(worker_thread_);
  }

  if (!network_manager_) {
    base::WaitableEvent event(true, false);
    chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &MediaStreamDependencyFactory::CreateIpcNetworkManagerOnWorkerThread,
        base::Unretained(this),
        &event));
    event.Wait();
  }

  if (!socket_factory_.get()) {
    socket_factory_.reset(
        new content::IpcPacketSocketFactory(p2p_socket_dispatcher_));
  }

  if (!CreatePeerConnectionFactory(
      worker_thread_,
      signaling_thread_,
      p2p_socket_dispatcher_,
      network_manager_,
      socket_factory_.get())) {
    LOG(ERROR) << "Could not create PeerConnection factory";
    return false;
  }
  return true;
}

void MediaStreamDependencyFactory::CleanupPeerConnectionFactory() {
  pc_factory_ = NULL;
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &MediaStreamDependencyFactory::DeleteIpcNetworkManager,
          base::Unretained(this)));
      // Stopping the thread will wait until all tasks have been
      // processed before returning. We wait for the above task to finish before
      // letting the the function continue to avoid any potential race issues.
      chrome_worker_thread_.Stop();
    } else {
      NOTREACHED() << "Worker thread not running.";
    }
  }
}
