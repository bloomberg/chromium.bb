// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "content/renderer/media/peer_connection_identity_service.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_capturer.h"
#include "content/renderer/media/rtc_video_decoder_factory.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging_initializer.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/render_thread_impl.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/filters/gpu_video_decoder_factories.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"

#if defined(USE_OPENSSL)
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#else
#include "net/socket/nss_ssl_util.h"
#endif

#if defined(GOOGLE_TV)
#include "content/renderer/media/rtc_video_decoder_factory_tv.h"
#endif

namespace content {

// The constraint key for the PeerConnection constructor for enabling diagnostic
// WebRTC logging. It's a Google specific key, hence the "goog" prefix.
const char kWebRtcLoggingConstraint[] = "googLog";

// Constant constraint keys which disables all audio constraints.
// Only used in combination with WebAudio sources.
struct {
  const char* key;
  const char* value;
} const kWebAudioConstraints[] = {
    {webrtc::MediaConstraintsInterface::kEchoCancellation,
     webrtc::MediaConstraintsInterface::kValueTrue},
    {webrtc::MediaConstraintsInterface::kAutoGainControl,
     webrtc::MediaConstraintsInterface::kValueTrue},
    {webrtc::MediaConstraintsInterface::kNoiseSuppression,
     webrtc::MediaConstraintsInterface::kValueTrue},
    {webrtc::MediaConstraintsInterface::kHighpassFilter,
     webrtc::MediaConstraintsInterface::kValueTrue},
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

  virtual ~WebAudioConstraints() {}
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
    config.legacy_relay = false;
    for (size_t i = 0; i < turn_configurations.size(); ++i) {
      P2PPortAllocator::Config::RelayServerConfig relay_config;
      relay_config.server_address = turn_configurations[i].server.hostname();
      relay_config.port = turn_configurations[i].server.port();
      relay_config.username = turn_configurations[i].username;
      relay_config.password = turn_configurations[i].password;
      relay_config.transport_type = turn_configurations[i].transport_type;
      relay_config.secure = turn_configurations[i].secure;
      config.relays.push_back(relay_config);
    }

    // Use first turn server as the stun server.
    if (turn_configurations.size() > 0) {
      config.stun_server = config.relays[0].server_address;
      config.stun_server_port = config.relays[0].port;
    }

    return new P2PPortAllocator(
        web_frame_, socket_dispatcher_.get(), network_manager_,
        socket_factory_, config);
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
      WebKit::WebMediaStream* web_stream,
      const MediaStreamDependencyFactory::MediaSourcesCreatedCallback& callback)
     : web_stream_(web_stream),
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
      ready_callback_.Run(web_stream_, live_);
      delete this;
    }
  }

  WebKit::WebMediaStream* web_stream_;
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
#if defined(GOOGLE_TV)
      decoder_factory_tv_(NULL),
#endif
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
    int render_view_id,
    const WebKit::WebMediaConstraints& audio_constraints,
    const WebKit::WebMediaConstraints& video_constraints,
    WebKit::WebMediaStream* web_stream,
    const MediaSourcesCreatedCallback& sources_created) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateNativeMediaSources()";
  if (!EnsurePeerConnectionFactory()) {
    sources_created.Run(web_stream, false);
    return;
  }

  // |source_observer| clean up itself when it has completed
  // source_observer->StartObservering.
  SourceStateObserver* source_observer =
      new SourceStateObserver(web_stream, sources_created);

  // Create local video sources.
  RTCMediaConstraints native_video_constraints(video_constraints);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  web_stream->videoTracks(video_tracks);
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
            content::MEDIA_DESKTOP_VIDEO_CAPTURE;
    source_data->SetVideoSource(
        CreateLocalVideoSource(source_data->device_info().session_id,
                               is_screencast,
                               &native_video_constraints).get());
    source_observer->AddSource(source_data->video_source());
  }

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  web_stream->audioTracks(audio_tracks);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableWebRtcAecRecordings)) {
    native_audio_constraints.AddOptional(
        RTCMediaConstraints::kInternalAecDump, "true");
  }
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    const WebKit::WebMediaStreamSource& source = audio_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());
    if (!source_data) {
      // TODO(henrika): Implement support for sources from remote MediaStreams.
      NOTIMPLEMENTED();
      continue;
    }

    // TODO(xians): Create a new capturer for difference microphones when we
    // support multiple microphones. See issue crbug/262117 .
    const StreamDeviceInfo device_info = source_data->device_info();
    scoped_refptr<WebRtcAudioCapturer> capturer(
        MaybeCreateAudioCapturer(render_view_id, device_info));
    if (!capturer.get()) {
      DLOG(WARNING) << "Failed to create the capturer for device "
                    << device_info.device.id;
      sources_created.Run(web_stream, false);
      return;
    }

    // Creates a LocalAudioSource object which holds audio options.
    // TODO(xians): The option should apply to the track instead of the source.
    source_data->SetLocalAudioSource(
        CreateLocalAudioSource(&native_audio_constraints).get());
    source_observer->AddSource(source_data->local_audio_source());
  }

  source_observer->StartObservering();
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    WebKit::WebMediaStream* web_stream) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateNativeLocalMediaStream()";
  if (!EnsurePeerConnectionFactory()) {
    DVLOG(1) << "EnsurePeerConnectionFactory() failed!";
    return;
  }

  std::string label = UTF16ToUTF8(web_stream->id());
  scoped_refptr<webrtc::MediaStreamInterface> native_stream =
      CreateLocalMediaStream(label);
  MediaStreamExtraData* extra_data =
      new MediaStreamExtraData(native_stream.get(), true);
  web_stream->setExtraData(extra_data);

  // Add audio tracks.
  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  web_stream->audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    AddNativeMediaStreamTrack(*web_stream, audio_tracks[i]);
  }

  // Add video tracks.
  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  web_stream->videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    AddNativeMediaStreamTrack(*web_stream, video_tracks[i]);
  }
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    WebKit::WebMediaStream* web_stream,
    const MediaStreamExtraData::StreamStopCallback& stream_stop) {
  CreateNativeLocalMediaStream(web_stream);

  MediaStreamExtraData* extra_data =
     static_cast<MediaStreamExtraData*>(web_stream->extraData());
  extra_data->SetLocalStreamStopCallback(stream_stop);
}

bool MediaStreamDependencyFactory::AddNativeMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& track) {
  MediaStreamExtraData* extra_data =
     static_cast<MediaStreamExtraData*>(stream.extraData());
  webrtc::MediaStreamInterface* native_stream = extra_data->stream().get();
  DCHECK(native_stream);

  WebKit::WebMediaStreamSource source = track.source();
  MediaStreamSourceExtraData* source_data =
      static_cast<MediaStreamSourceExtraData*>(source.extraData());

  scoped_refptr<WebRtcAudioCapturer> capturer;
  if (!source_data) {
    if (source.requiresAudioConsumer()) {
      // We're adding a WebAudio MediaStream.
      // Create a specific capturer for each WebAudio consumer.
      capturer = CreateWebAudioSource(&source);
      source_data =
          static_cast<MediaStreamSourceExtraData*>(source.extraData());
    } else {
      // TODO(perkj): Implement support for sources from
      // remote MediaStreams.
      NOTIMPLEMENTED();
      return false;
    }
  }

  WebKit::WebMediaStreamSource::Type type = track.source().type();
  DCHECK(type == WebKit::WebMediaStreamSource::TypeAudio ||
         type == WebKit::WebMediaStreamSource::TypeVideo);

  std::string track_id = UTF16ToUTF8(track.id());
  if (source.type() == WebKit::WebMediaStreamSource::TypeAudio) {
    if (!capturer.get() && GetWebRtcAudioDevice())
      capturer = GetWebRtcAudioDevice()->GetDefaultCapturer();

    scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        CreateLocalAudioTrack(track_id,
                              capturer,
                              source_data->local_audio_source()));
    audio_track->set_enabled(track.isEnabled());
    return native_stream->AddTrack(audio_track.get());
  } else {
    DCHECK(source.type() == WebKit::WebMediaStreamSource::TypeVideo);
    scoped_refptr<webrtc::VideoTrackInterface> video_track(
        CreateLocalVideoTrack(track_id, source_data->video_source()));
    video_track->set_enabled(track.isEnabled());
    return native_stream->AddTrack(video_track.get());
  }
}

bool MediaStreamDependencyFactory::AddNativeVideoMediaTrack(
    const std::string& track_id,
    WebKit::WebMediaStream* stream,
    cricket::VideoCapturer* capturer) {
  if (!stream) {
    LOG(ERROR) << "AddNativeVideoMediaTrack called with null WebMediaStream.";
    return false;
  }

  // Create native track from the source.
  scoped_refptr<webrtc::VideoTrackInterface> native_track =
      CreateLocalVideoTrack(track_id, capturer);

  // Add the native track to native stream
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream->extraData());
  DCHECK(extra_data);
  webrtc::MediaStreamInterface* native_stream = extra_data->stream().get();
  native_stream->AddTrack(native_track.get());

  // Create a new webkit video track.
  WebKit::WebMediaStreamTrack webkit_track;
  WebKit::WebMediaStreamSource webkit_source;
  WebKit::WebString webkit_track_id(UTF8ToUTF16(track_id));
  WebKit::WebMediaStreamSource::Type type =
      WebKit::WebMediaStreamSource::TypeVideo;
  webkit_source.initialize(webkit_track_id, type, webkit_track_id);
  webkit_track.initialize(webkit_track_id, webkit_source);

  // Add the track to WebMediaStream.
  stream->addTrack(webkit_track);
  return true;
}

bool MediaStreamDependencyFactory::RemoveNativeMediaStreamTrack(
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& track) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  webrtc::MediaStreamInterface* native_stream = extra_data->stream().get();
  DCHECK(native_stream);

  WebKit::WebMediaStreamSource::Type type = track.source().type();
  DCHECK(type == WebKit::WebMediaStreamSource::TypeAudio ||
         type == WebKit::WebMediaStreamSource::TypeVideo);

  std::string track_id = UTF16ToUTF8(track.id());
  return type == WebKit::WebMediaStreamSource::TypeAudio ?
      native_stream->RemoveTrack(native_stream->FindAudioTrack(track_id)) :
      native_stream->RemoveTrack(native_stream->FindVideoTrack(track_id));
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory() {
  DVLOG(1) << "MediaStreamDependencyFactory::CreatePeerConnectionFactory()";
  if (!pc_factory_.get()) {
    DCHECK(!audio_device_.get());
    audio_device_ = new WebRtcAudioDeviceImpl();

    scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;

    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch(switches::kEnableWebRtcHWDecoding)) {
      scoped_refptr<base::MessageLoopProxy> media_loop_proxy =
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy();
      scoped_refptr<RendererGpuVideoDecoderFactories> gpu_factories =
          RenderThreadImpl::current()->GetGpuFactories(media_loop_proxy);
      if (gpu_factories.get() != NULL)
        decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));
    }
#if defined(GOOGLE_TV)
    // PeerConnectionFactory will hold the ownership of this
    // VideoDecoderFactory.
    decoder_factory.reset(decoder_factory_tv_ = new RTCVideoDecoderFactoryTv);
#endif

    scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
        webrtc::CreatePeerConnectionFactory(worker_thread_,
                                            signaling_thread_,
                                            audio_device_.get(),
                                            NULL,
                                            decoder_factory.release()));
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

  webrtc::MediaConstraintsInterface::Constraints optional_constraints =
      constraints->GetOptional();
  std::string constraint_value;
  if (optional_constraints.FindFirst(kWebRtcLoggingConstraint,
                                     &constraint_value)) {
    std::string url = web_frame->document().url().spec();
    RenderThreadImpl::current()->GetIOMessageLoopProxy()->PostTask(
        FROM_HERE, base::Bind(
            &InitWebRtcLogging,
            constraint_value,
            url));
  }

  scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            p2p_socket_dispatcher_.get(),
            network_manager_,
            socket_factory_.get(),
            web_frame);

  PeerConnectionIdentityService* identity_service =
      PeerConnectionIdentityService::Create(
          GURL(web_frame->document().url().spec()).GetOrigin());

  return pc_factory_->CreatePeerConnection(ice_servers,
                                           constraints,
                                           pa_factory.get(),
                                           identity_service,
                                           observer).get();
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

scoped_refptr<WebRtcAudioCapturer>
MediaStreamDependencyFactory::CreateWebAudioSource(
    WebKit::WebMediaStreamSource* source) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateWebAudioSource()";
  DCHECK(GetWebRtcAudioDevice());

  // Set up the source and ensure that WebAudio is driving things instead of
  // a microphone. For WebAudio, we always create a new capturer without
  // calling initialize(), WebAudio will re-configure the capturer later on.
  // Pass -1 as the |render_view_id| and an empty device struct to tell the
  // capturer not to start the default source.
  scoped_refptr<WebRtcAudioCapturer> capturer(
      MaybeCreateAudioCapturer(-1, StreamDeviceInfo()));
  DCHECK(capturer.get());

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource(capturer.get()));
  MediaStreamSourceExtraData* source_data =
      new content::MediaStreamSourceExtraData(webaudio_capturer_source.get());

  // Create a LocalAudioSource object which holds audio options.
  // Use audio constraints where all values are false, i.e., disable
  // echo cancellation, automatic gain control, noise suppression and
  // high-pass filter. SetLocalAudioSource() affects core audio parts in
  // third_party/Libjingle.
  WebAudioConstraints webaudio_audio_constraints_all_false;
  source_data->SetLocalAudioSource(
      CreateLocalAudioSource(&webaudio_audio_constraints_all_false).get());
  source->setExtraData(source_data);

  // Replace the default source with WebAudio as source instead.
  source->addAudioConsumer(webaudio_capturer_source.get());

  return capturer;
}

scoped_refptr<webrtc::VideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  return pc_factory_->CreateVideoTrack(id, source).get();
}

scoped_refptr<webrtc::VideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id, cricket::VideoCapturer* capturer) {
  if (!capturer) {
    LOG(ERROR) << "CreateLocalVideoTrack called with null VideoCapturer.";
    return NULL;
  }

  // Create video source from the |capturer|.
  scoped_refptr<webrtc::VideoSourceInterface> source =
      pc_factory_->CreateVideoSource(capturer, NULL).get();

  // Create native track from the source.
  return pc_factory_->CreateVideoTrack(id, source.get()).get();
}

scoped_refptr<webrtc::AudioTrackInterface>
MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& id,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    webrtc::AudioSourceInterface* source) {
  // TODO(xians): Merge |source| to the capturer(). We can't do this today
  // because only one capturer() is supported while one |source| is created
  // for each audio track.
  scoped_refptr<WebRtcLocalAudioTrack> audio_track(
      WebRtcLocalAudioTrack::Create(id, capturer, source));
  // Add the WebRtcAudioDevice as the sink to the local audio track.
  audio_track->AddSink(GetWebRtcAudioDevice());
  // Start the audio track. This will hook the |audio_track| to the capturer
  // as the sink of the audio, and only start the source of the capturer if
  // it is the first audio track connecting to the capturer.
  audio_track->Start();
  return audio_track;
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
  return audio_device_.get();
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
  DCHECK_EQ(base::MessageLoop::current(), chrome_worker_thread_.message_loop());
  network_manager_ = new IpcNetworkManager(p2p_socket_dispatcher_.get());
  event->Signal();
}

void MediaStreamDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK_EQ(base::MessageLoop::current(), chrome_worker_thread_.message_loop());
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

  if (!socket_factory_) {
    socket_factory_.reset(
        new IpcPacketSocketFactory(p2p_socket_dispatcher_.get()));
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

scoped_refptr<WebRtcAudioCapturer>
MediaStreamDependencyFactory::MaybeCreateAudioCapturer(
    int render_view_id,
    const StreamDeviceInfo& device_info) {
  scoped_refptr<WebRtcAudioCapturer> capturer;
  if (render_view_id != -1) {
    // From a normal getUserMedia, re-use the existing default capturer.
    capturer = GetWebRtcAudioDevice()->GetDefaultCapturer();
  }
  // If the default capturer does not exist or |render_view_id| == -1, create
  // a new capturer.
  bool is_new_capturer = false;
  if (!capturer.get()) {
    capturer = WebRtcAudioCapturer::CreateCapturer();
    is_new_capturer = true;
  }

  if (!capturer->Initialize(
          render_view_id,
          static_cast<media::ChannelLayout>(device_info.device.channel_layout),
          device_info.device.sample_rate, device_info.session_id,
          device_info.device.id)) {
    return NULL;
  }

  // Add the capturer to the WebRtcAudioDeviceImpl if it is a new capturer.
  if (is_new_capturer)
    GetWebRtcAudioDevice()->AddAudioCapturer(capturer);

  return capturer;
}

}  // namespace content
