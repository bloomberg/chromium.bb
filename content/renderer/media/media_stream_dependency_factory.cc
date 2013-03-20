// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_capturer.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "jingle/glue/thread_wrapper.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

#if defined(USE_OPENSSL)
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#else
#include "net/socket/nss_ssl_util.h"
#endif

namespace content {

// Constant constraint keys which disables all audio constraints.
// Only used in combination with WebAudio sources.
struct {
  const char* key;
  const char* value;
} const kWebAudioConstraints[] = {
  {webrtc::MediaConstraintsInterface::kEchoCancellation,
   webrtc::MediaConstraintsInterface::kValueFalse},
  {webrtc::MediaConstraintsInterface::kAutoGainControl,
   webrtc::MediaConstraintsInterface::kValueFalse},
  {webrtc::MediaConstraintsInterface::kNoiseSuppression,
   webrtc::MediaConstraintsInterface::kValueFalse},
  {webrtc::MediaConstraintsInterface::kHighpassFilter,
   webrtc::MediaConstraintsInterface::kValueFalse},
};

class WebAudioConstraints : public RTCMediaConstraints {
 public:
  WebAudioConstraints()
      : RTCMediaConstraints(WebKit::WebMediaConstraints()) {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kWebAudioConstraints); ++i) {
      webrtc::MediaConstraintsInterface::Constraint constraint;
      constraint.key = kWebAudioConstraints[i].key;
      constraint.value = kWebAudioConstraints[i].value;

      DVLOG(1) << "WebAudioConstraints: " << constraint.key
               << " : " <<  constraint.value;
      mandatory_.push_back(constraint);
    }
  }

  virtual ~WebAudioConstraints() {};
};

class P2PPortAllocatorFactory : public webrtc::PortAllocatorFactoryInterface {
 public:
  P2PPortAllocatorFactory(
      P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory,
      WebKit::WebFrame* web_frame)
      : socket_dispatcher_(socket_dispatcher),
        network_manager_(network_manager),
        socket_factory_(socket_factory),
        web_frame_(web_frame) {
  }

  virtual cricket::PortAllocator* CreatePortAllocator(
      const std::vector<StunConfiguration>& stun_servers,
      const std::vector<TurnConfiguration>& turn_configurations) OVERRIDE {
    CHECK(web_frame_);
    P2PPortAllocator::Config config;
    if (stun_servers.size() > 0) {
      config.stun_server = stun_servers[0].server.hostname();
      config.stun_server_port = stun_servers[0].server.port();
    }
    if (turn_configurations.size() > 0) {
      config.legacy_relay = false;
      config.relay_server = turn_configurations[0].server.hostname();
      config.relay_server_port = turn_configurations[0].server.port();
      config.relay_username = turn_configurations[0].username;
      config.relay_password = turn_configurations[0].password;
      // Use the turn server as the stun server.
      config.stun_server = config.relay_server;
      config.stun_server_port = config.relay_server_port;
    }

    return new P2PPortAllocator(web_frame_,
                                socket_dispatcher_,
                                network_manager_,
                                socket_factory_,
                                config);
  }

 protected:
  virtual ~P2PPortAllocatorFactory() {}

 private:
  scoped_refptr<P2PSocketDispatcher> socket_dispatcher_;
  // |network_manager_| and |socket_factory_| are a weak references, owned by
  // MediaStreamDependencyFactory.
  talk_base::NetworkManager* network_manager_;
  talk_base::PacketSocketFactory* socket_factory_;
  // Raw ptr to the WebFrame that created the P2PPortAllocatorFactory.
  WebKit::WebFrame* web_frame_;
};

// SourceStateObserver is a help class used for observing the startup state
// transition of webrtc media sources such as a camera or microphone.
// An instance of the object deletes itself after use.
// Usage:
// 1. Create an instance of the object with the WebKit::WebMediaStream
//    the observed sources belongs to a callback.
// 2. Add the sources to the observer using AddSource.
// 3. Call StartObserving()
// 4. The callback will be triggered when all sources have transitioned from
//    webrtc::MediaSourceInterface::kInitializing.
class SourceStateObserver : public webrtc::ObserverInterface,
                            public base::NonThreadSafe {
 public:
  SourceStateObserver(
      WebKit::WebMediaStream* description,
      const MediaStreamDependencyFactory::MediaSourcesCreatedCallback& callback)
     : description_(description),
       ready_callback_(callback),
       live_(true) {
  }

  void AddSource(webrtc::MediaSourceInterface* source) {
    DCHECK(CalledOnValidThread());
    switch (source->state()) {
      case webrtc::MediaSourceInterface::kInitializing:
        sources_.push_back(source);
        source->RegisterObserver(this);
        break;
      case webrtc::MediaSourceInterface::kLive:
        // The source is already live so we don't need to wait for it.
        break;
      case webrtc::MediaSourceInterface::kEnded:
        // The source have already failed.
        live_ = false;
        break;
      default:
        NOTREACHED();
    }
  }

  void StartObservering() {
    DCHECK(CalledOnValidThread());
    CheckIfSourcesAreLive();
  }

  virtual void OnChanged() OVERRIDE {
    DCHECK(CalledOnValidThread());
    CheckIfSourcesAreLive();
  }

 private:
  void CheckIfSourcesAreLive() {
    ObservedSources::iterator it = sources_.begin();
    while (it != sources_.end()) {
      if ((*it)->state() != webrtc::MediaSourceInterface::kInitializing) {
        live_ &=  (*it)->state() == webrtc::MediaSourceInterface::kLive;
        (*it)->UnregisterObserver(this);
        it = sources_.erase(it);
      } else {
        ++it;
      }
    }
    if (sources_.empty()) {
      ready_callback_.Run(description_, live_);
      delete this;
    }
  }

  WebKit::WebMediaStream* description_;
  MediaStreamDependencyFactory::MediaSourcesCreatedCallback ready_callback_;
  bool live_;
  typedef std::vector<scoped_refptr<webrtc::MediaSourceInterface> >
      ObservedSources;
  ObservedSources sources_;
};

MediaStreamDependencyFactory::MediaStreamDependencyFactory(
    VideoCaptureImplManager* vc_manager,
    P2PSocketDispatcher* p2p_socket_dispatcher)
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

void MediaStreamDependencyFactory::CreateNativeMediaSources(
    const WebKit::WebMediaConstraints& audio_constraints,
    const WebKit::WebMediaConstraints& video_constraints,
    WebKit::WebMediaStream* description,
    const MediaSourcesCreatedCallback& sources_created) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateNativeMediaSources()";
  if (!EnsurePeerConnectionFactory()) {
    sources_created.Run(description, false);
    return;
  }

  // |source_observer| clean up itself when it has completed
  // source_observer->StartObservering.
  SourceStateObserver* source_observer =
      new SourceStateObserver(description, sources_created);

  // Create local video sources.
  RTCMediaConstraints native_video_constraints(video_constraints);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  description->videoSources(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = video_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }
    const bool is_screencast =
        source_data->device_info().device.type ==
            content::MEDIA_TAB_VIDEO_CAPTURE ||
        source_data->device_info().device.type ==
            content::MEDIA_SCREEN_VIDEO_CAPTURE;
    source_data->SetVideoSource(
        CreateLocalVideoSource(source_data->device_info().session_id,
                               is_screencast,
                               &native_video_constraints));
    source_observer->AddSource(source_data->video_source());
  }

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  description->audioSources(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = audio_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(henrika): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }

    const StreamDeviceInfo device_info = source_data->device_info();
    if (IsAudioMediaType(device_info.device.type)) {
      if (!InitializeAudioSource(device_info)) {
        DLOG(WARNING) << "Unsupported audio source";
        sources_created.Run(description, false);
        return;
      }
    }

    // Creates a LocalAudioSource object which holds audio options.
    source_data->SetLocalAudioSource(
        CreateLocalAudioSource(&native_audio_constraints));
    source_observer->AddSource(source_data->local_audio_source());
  }

  source_observer->StartObservering();
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    WebKit::WebMediaStream* description) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateNativeLocalMediaStream()";
  if (!EnsurePeerConnectionFactory()) {
      DVLOG(1) << "EnsurePeerConnectionFactory() failed!";
      return;
  }

  std::string label = UTF16ToUTF8(description->label());
  scoped_refptr<webrtc::MediaStreamInterface> native_stream =
      CreateLocalMediaStream(label);

  // Add audio tracks.
  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  description->audioSources(audio_tracks);

  bool start_stream = false;
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    WebKit::WebMediaStreamSource source = audio_tracks[i].source();

    // See if we're adding a WebAudio MediaStream.
    if (source.requiresAudioConsumer()) {
      // TODO(crogers, xians): In reality we should be able to send a unique
      // audio stream to each PeerConnection separately.  But currently WebRTC
      // is only able to handle a global audio stream sent to ALL peers.

      // Create a special source where default WebAudio constraints are used.
      if (!CreateWebAudioSource(&source)) {
        LOG(ERROR) << "Failed to create WebAudio source";
        continue;
      }
    }

    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());

    if (!source_data) {
      // TODO(perkj): Implement support for sources from
      // remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }

    scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        CreateLocalAudioTrack(UTF16ToUTF8(audio_tracks[i].id()),
                              source_data->local_audio_source()));
    native_stream->AddTrack(audio_track);
    audio_track->set_enabled(audio_tracks[i].isEnabled());
    start_stream = true;
  }

  if (start_stream && GetWebRtcAudioDevice()) {
    WebRtcAudioCapturer* capturer = GetWebRtcAudioDevice()->capturer();
    capturer->Start();
  }

  // Add video tracks.
  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  description->videoSources(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = video_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data || !source_data->video_source()) {
      // TODO(perkj): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }

    scoped_refptr<webrtc::VideoTrackInterface> video_track(
        CreateLocalVideoTrack(UTF16ToUTF8(video_tracks[i].id()),
                              source_data->video_source()));

    native_stream->AddTrack(video_track);
    video_track->set_enabled(video_tracks[i].isEnabled());
  }

  MediaStreamExtraData* extra_data = new MediaStreamExtraData(native_stream,
                                                              true);
  description->setExtraData(extra_data);
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    WebKit::WebMediaStream* description,
    const MediaStreamExtraData::StreamStopCallback& stream_stop) {
  CreateNativeLocalMediaStream(description);

  MediaStreamExtraData* extra_data =
     static_cast<MediaStreamExtraData*>(description->extraData());
  extra_data->SetLocalStreamStopCallback(stream_stop);
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory() {
  DVLOG(1) << "MediaStreamDependencyFactory::CreatePeerConnectionFactory()";
  if (!pc_factory_.get()) {
    DCHECK(!audio_device_);
    audio_device_ = new WebRtcAudioDeviceImpl();
    scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
        webrtc::CreatePeerConnectionFactory(worker_thread_,
                                            signaling_thread_,
                                            audio_device_));
    if (factory.get())
      pc_factory_ = factory;
    else
      audio_device_ = NULL;
  }
  return pc_factory_.get() != NULL;
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::IceServers& ice_servers,
    const webrtc::MediaConstraintsInterface* constraints,
    WebKit::WebFrame* web_frame,
    webrtc::PeerConnectionObserver* observer) {
  CHECK(web_frame);
  CHECK(observer);
  scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            p2p_socket_dispatcher_.get(),
            network_manager_,
            socket_factory_.get(),
            web_frame);
  return pc_factory_->CreatePeerConnection(
      ice_servers, constraints, pa_factory, observer).get();
}

scoped_refptr<webrtc::MediaStreamInterface>
MediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return pc_factory_->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::AudioSourceInterface>
MediaStreamDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  scoped_refptr<webrtc::AudioSourceInterface> source =
      pc_factory_->CreateAudioSource(constraints).get();
  return source;
}

scoped_refptr<webrtc::VideoSourceInterface>
MediaStreamDependencyFactory::CreateLocalVideoSource(
    int video_session_id,
    bool is_screencast,
    const webrtc::MediaConstraintsInterface* constraints) {
  RtcVideoCapturer* capturer = new RtcVideoCapturer(
      video_session_id, vc_manager_.get(), is_screencast);

  // The video source takes ownership of |capturer|.
  scoped_refptr<webrtc::VideoSourceInterface> source =
      pc_factory_->CreateVideoSource(capturer, constraints).get();
  return source;
}

bool MediaStreamDependencyFactory::InitializeAudioSource(
  const StreamDeviceInfo& device_info) {
  DVLOG(1) << "MediaStreamDependencyFactory::InitializeAudioSource()";

  // TODO(henrika): the current design does not support a unique source
  // for each audio track.
  if (device_info.session_id <= 0)
    return false;

  // Initialize the source using audio parameters for the selected
  // capture device.
  WebRtcAudioCapturer* capturer = GetWebRtcAudioDevice()->capturer();
  // TODO(henrika): refactor \content\public\common\media_stream_request.h
  // to allow dependency of media::ChannelLayout and avoid static_cast.
  if (!capturer->Initialize(
          static_cast<media::ChannelLayout>(device_info.device.channel_layout),
          device_info.device.sample_rate, device_info.session_id))
    return false;

  return true;
}

bool MediaStreamDependencyFactory::CreateWebAudioSource(
    WebKit::WebMediaStreamSource* source) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateWebAudioSource()";
  DCHECK(GetWebRtcAudioDevice());

  // WebAudio needs the WebRtcAudioCapturer to be able to send its data
  // over a PeerConnection. The microphone source is not utilized in this
  // case; instead the WebRtcAudioCapturer is driving.
  WebRtcAudioCapturer* capturer = GetWebRtcAudioDevice()->capturer();
  if (!capturer)
    return false;

  // Set up the source and ensure that WebAudio is driving things instead of
  // a microphone.

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource(capturer));
  MediaStreamSourceExtraData* source_data =
      new content::MediaStreamSourceExtraData(webaudio_capturer_source);

  // Create a LocalAudioSource object which holds audio options.
  // Use audio constraints where all values are false, i.e., disable
  // echo cancellation, automatic gain control, noise suppression and
  // high-pass filter. SetLocalAudioSource() affects core audio parts in
  // third_party/Libjingle.
  WebAudioConstraints webaudio_audio_constraints_all_false;
  source_data->SetLocalAudioSource(
      CreateLocalAudioSource(&webaudio_audio_constraints_all_false));
  source->setExtraData(source_data);

  // Replace the default source with WebAudio as source instead.
  source->addAudioConsumer(webaudio_capturer_source);

  return true;
}

scoped_refptr<webrtc::VideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  return pc_factory_->CreateVideoTrack(id, source).get();
}

scoped_refptr<webrtc::AudioTrackInterface>
MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& id,
    webrtc::AudioSourceInterface* source) {
  return pc_factory_->CreateAudioTrack(id, source).get();
}

webrtc::SessionDescriptionInterface*
MediaStreamDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return webrtc::CreateSessionDescription(type, sdp, error);
}

webrtc::IceCandidateInterface* MediaStreamDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

WebRtcAudioDeviceImpl*
MediaStreamDependencyFactory::GetWebRtcAudioDevice() {
  return audio_device_;
}

void MediaStreamDependencyFactory::StopLocalAudioSource(
    const WebKit::WebMediaStream& description) {
  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      description.extraData());
  if (extra_data && extra_data->is_local() && extra_data->stream() &&
      !extra_data->stream()->GetAudioTracks().empty()) {
    if (GetWebRtcAudioDevice()) {
      scoped_refptr<WebRtcAudioCapturer> capturer =
          GetWebRtcAudioDevice()->capturer();
      if (capturer)
        capturer->Stop();
    }
  }
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
  network_manager_ = new IpcNetworkManager(p2p_socket_dispatcher_);
  event->Signal();
}

void MediaStreamDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK_EQ(MessageLoop::current(), chrome_worker_thread_.message_loop());
  delete network_manager_;
  network_manager_ = NULL;
}

bool MediaStreamDependencyFactory::EnsurePeerConnectionFactory() {
  DCHECK(CalledOnValidThread());
  if (PeerConnectionFactoryCreated())
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
        new IpcPacketSocketFactory(p2p_socket_dispatcher_));
  }

  // Init SSL, which will be needed by PeerConnection.
#if defined(USE_OPENSSL)
  if (!talk_base::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    return false;
  }
#else
  // TODO(ronghuawu): Replace this call with InitializeSSL.
  net::EnsureNSSSSLInit();
#endif

  if (!CreatePeerConnectionFactory()) {
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

}  // namespace content
