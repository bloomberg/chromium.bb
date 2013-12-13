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
#include "content/renderer/media/media_stream_track_extra_data.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_identity_service.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_capturer.h"
#include "content/renderer/media/rtc_video_decoder_factory.h"
#include "content/renderer/media/rtc_video_encoder_factory.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/render_thread_impl.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/filters/gpu_video_accelerator_factories.h"
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

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_bridge.h"
#endif

namespace content {

// Constant constraint keys which enables default audio constraints on
// mediastreams with audio.
struct {
  const char* key;
  const char* value;
} const kDefaultAudioConstraints[] = {
  { webrtc::MediaConstraintsInterface::kEchoCancellation,
    webrtc::MediaConstraintsInterface::kValueTrue },
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // Enable the extended filter mode AEC on platforms with known echo issues.
  { webrtc::MediaConstraintsInterface::kExperimentalEchoCancellation,
    webrtc::MediaConstraintsInterface::kValueTrue },
#endif
  { webrtc::MediaConstraintsInterface::kAutoGainControl,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kExperimentalAutoGainControl,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kNoiseSuppression,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kHighpassFilter,
    webrtc::MediaConstraintsInterface::kValueTrue },
};

// Map of corresponding media constraints and platform effects.
struct {
  const char* constraint;
  const media::AudioParameters::PlatformEffectsMask effect;
} const kConstraintEffectMap[] = {
  { webrtc::MediaConstraintsInterface::kEchoCancellation,
    media::AudioParameters::ECHO_CANCELLER},
};

// Merge |constraints| with |kDefaultAudioConstraints|. For any key which exists
// in both, the value from |constraints| is maintained, including its
// mandatory/optional status. New values from |kDefaultAudioConstraints| will
// be added with mandatory status.
void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDefaultAudioConstraints); ++i) {
    bool already_set_value;
    if (!webrtc::FindConstraint(constraints, kDefaultAudioConstraints[i].key,
                                &already_set_value, NULL)) {
      constraints->AddMandatory(kDefaultAudioConstraints[i].key,
          kDefaultAudioConstraints[i].value, false);
    } else {
      DVLOG(1) << "Constraint " << kDefaultAudioConstraints[i].key
               << " already set to " << already_set_value;
    }
  }
}

class P2PPortAllocatorFactory : public webrtc::PortAllocatorFactoryInterface {
 public:
  P2PPortAllocatorFactory(
      P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory,
      blink::WebFrame* web_frame)
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
  blink::WebFrame* web_frame_;
};

// SourceStateObserver is a help class used for observing the startup state
// transition of webrtc media sources such as a camera or microphone.
// An instance of the object deletes itself after use.
// Usage:
// 1. Create an instance of the object with the blink::WebMediaStream
//    the observed sources belongs to a callback.
// 2. Add the sources to the observer using AddSource.
// 3. Call StartObserving()
// 4. The callback will be triggered when all sources have transitioned from
//    webrtc::MediaSourceInterface::kInitializing.
class SourceStateObserver : public webrtc::ObserverInterface,
                            public base::NonThreadSafe {
 public:
  SourceStateObserver(
      blink::WebMediaStream* web_stream,
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

  blink::WebMediaStream* web_stream_;
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

blink::WebRTCPeerConnectionHandler*
MediaStreamDependencyFactory::CreateRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
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
    const blink::WebMediaConstraints& audio_constraints,
    const blink::WebMediaConstraints& video_constraints,
    blink::WebMediaStream* web_stream,
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
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream->videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    const blink::WebMediaStreamSource& source = video_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());

    // Check if the source has already been created. This happens when the same
    // source is used in multiple MediaStreams as a result of calling
    // getUserMedia.
    if (source_data->video_source())
      continue;

    const bool is_screencast =
        source_data->device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
        source_data->device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE;
    source_data->SetVideoSource(
        CreateLocalVideoSource(source_data->device_info().session_id,
                               is_screencast,
                               &native_video_constraints).get());
    source_observer->AddSource(source_data->video_source());
  }

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  ApplyFixedAudioConstraints(&native_audio_constraints);
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream->audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    const blink::WebMediaStreamSource& source = audio_tracks[i].source();
    MediaStreamSourceExtraData* source_data =
        static_cast<MediaStreamSourceExtraData*>(source.extraData());

    // Check if the source has already been created. This happens when the same
    // source is used in multiple MediaStreams as a result of calling
    // getUserMedia.
    if (source_data->local_audio_source())
      continue;

    // TODO(xians): Create a new capturer for difference microphones when we
    // support multiple microphones. See issue crbug/262117 .
    StreamDeviceInfo device_info = source_data->device_info();
    RTCMediaConstraints constraints = native_audio_constraints;

    // If any platform effects are available, check them against the
    // constraints. Disable effects to match false constraints, but if a
    // constraint is true, set the constraint to false to later disable the
    // software effect.
    int effects = device_info.device.input.effects;
    if (effects != media::AudioParameters::NO_EFFECTS) {
      for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kConstraintEffectMap); ++i) {
        bool value;
        if (!webrtc::FindConstraint(&constraints,
                kConstraintEffectMap[i].constraint, &value, NULL) || !value) {
          // If the constraint is false, or does not exist, disable the platform
          // effect.
          effects &= ~kConstraintEffectMap[i].effect;
          DVLOG(1) << "Disabling constraint: "
                   << kConstraintEffectMap[i].constraint;
        } else if (effects & kConstraintEffectMap[i].effect) {
          // If the constraint is true, leave the platform effect enabled, and
          // set the constraint to false to later disable the software effect.
          constraints.AddMandatory(kConstraintEffectMap[i].constraint,
              webrtc::MediaConstraintsInterface::kValueFalse, true);
          DVLOG(1) << "Disabling platform effect: "
                   << kConstraintEffectMap[i].constraint;
        }
      }
      device_info.device.input.effects = effects;
    }

    scoped_refptr<WebRtcAudioCapturer> capturer(
        MaybeCreateAudioCapturer(render_view_id, device_info));
    if (!capturer.get()) {
      DLOG(WARNING) << "Failed to create the capturer for device "
                    << device_info.device.id;
      sources_created.Run(web_stream, false);
      // TODO(xians): Don't we need to check if source_observer is observing
      // something? If not, then it looks like we have a leak here.
      // OTOH, if it _is_ observing something, then the callback might
      // be called multiple times which is likely also a bug.
      return;
    }
    source_data->SetAudioCapturer(capturer);

    // Creates a LocalAudioSource object which holds audio options.
    // TODO(xians): The option should apply to the track instead of the source.
    source_data->SetLocalAudioSource(
        CreateLocalAudioSource(&constraints).get());
    source_observer->AddSource(source_data->local_audio_source());
  }

  source_observer->StartObservering();
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    blink::WebMediaStream* web_stream) {
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
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream->audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    AddNativeMediaStreamTrack(*web_stream, audio_tracks[i]);
  }

  // Add video tracks.
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream->videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    AddNativeMediaStreamTrack(*web_stream, video_tracks[i]);
  }
}

void MediaStreamDependencyFactory::CreateNativeLocalMediaStream(
    blink::WebMediaStream* web_stream,
    const MediaStreamExtraData::StreamStopCallback& stream_stop) {
  CreateNativeLocalMediaStream(web_stream);

  MediaStreamExtraData* extra_data =
     static_cast<MediaStreamExtraData*>(web_stream->extraData());
  extra_data->SetLocalStreamStopCallback(stream_stop);
}

scoped_refptr<webrtc::AudioTrackInterface>
MediaStreamDependencyFactory::CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  MediaStreamSourceExtraData* source_data =
      static_cast<MediaStreamSourceExtraData*>(source.extraData());

  // In the future the constraints will belong to the track itself, but
  // right now they're on the source, so we fetch them from there.
  RTCMediaConstraints track_constraints(source.constraints());

  // Apply default audio constraints that enable echo cancellation,
  // automatic gain control, noise suppression and high-pass filter.
  ApplyFixedAudioConstraints(&track_constraints);

  scoped_refptr<WebAudioCapturerSource> webaudio_source;
  if (!source_data) {
    if (source.requiresAudioConsumer()) {
      // We're adding a WebAudio MediaStream.
      // Create a specific capturer for each WebAudio consumer.
      webaudio_source = CreateWebAudioSource(&source, &track_constraints);
      source_data =
          static_cast<MediaStreamSourceExtraData*>(source.extraData());
    } else {
      // TODO(perkj): Implement support for sources from
      // remote MediaStreams.
      NOTIMPLEMENTED();
      return NULL;
    }
  }

  std::string track_id = UTF16ToUTF8(track.id());
  scoped_refptr<WebRtcAudioCapturer> capturer;
  if (GetWebRtcAudioDevice())
    capturer = GetWebRtcAudioDevice()->GetDefaultCapturer();

  scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      CreateLocalAudioTrack(track_id,
                            capturer,
                            webaudio_source.get(),
                            source_data->local_audio_source(),
                            &track_constraints));
  AddNativeTrackToBlinkTrack(audio_track.get(), track, true);

  audio_track->set_enabled(track.isEnabled());

  // Pass the pointer of the source provider to the blink audio track.
  blink::WebMediaStreamTrack writable_track = track;
  writable_track.setSourceProvider(static_cast<WebRtcLocalAudioTrack*>(
      audio_track.get())->audio_source_provider());

  return audio_track;
}

scoped_refptr<webrtc::VideoTrackInterface>
MediaStreamDependencyFactory::CreateNativeVideoMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeVideo);
  MediaStreamSourceExtraData* source_data =
      static_cast<MediaStreamSourceExtraData*>(source.extraData());

  if (!source_data) {
    // TODO(perkj): Implement support for sources from
    // remote MediaStreams.
    NOTIMPLEMENTED();
    return NULL;
  }

  std::string track_id = UTF16ToUTF8(track.id());
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      CreateLocalVideoTrack(track_id, source_data->video_source()));
  AddNativeTrackToBlinkTrack(video_track.get(), track, true);

  video_track->set_enabled(track.isEnabled());

  return video_track;
}

void MediaStreamDependencyFactory::CreateNativeMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DCHECK(!track.isNull() && !track.extraData());
  DCHECK(!track.source().isNull());

  switch (track.source().type()) {
    case blink::WebMediaStreamSource::TypeAudio:
      CreateNativeAudioMediaStreamTrack(track);
      break;
    case blink::WebMediaStreamSource::TypeVideo:
      CreateNativeVideoMediaStreamTrack(track);
      break;
  }
}

bool MediaStreamDependencyFactory::AddNativeMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  DCHECK(native_stream);

  switch (track.source().type()) {
    case blink::WebMediaStreamSource::TypeAudio: {
      scoped_refptr<webrtc::AudioTrackInterface> native_audio_track;
      if (!track.extraData()) {
        native_audio_track = CreateNativeAudioMediaStreamTrack(track);
      } else {
        native_audio_track = static_cast<webrtc::AudioTrackInterface*>(
            GetNativeMediaStreamTrack(track));
      }

      return native_audio_track.get() &&
          native_stream->AddTrack(native_audio_track);
    }
    case blink::WebMediaStreamSource::TypeVideo: {
      scoped_refptr<webrtc::VideoTrackInterface> native_video_track;
      if (!track.extraData()) {
        native_video_track = CreateNativeVideoMediaStreamTrack(track);
      } else {
        native_video_track = static_cast<webrtc::VideoTrackInterface*>(
            GetNativeMediaStreamTrack(track));
      }

      return native_video_track.get() &&
          native_stream->AddTrack(native_video_track);
    }
  }
  return false;
}

bool MediaStreamDependencyFactory::AddNativeVideoMediaTrack(
    const std::string& track_id,
    blink::WebMediaStream* stream,
    cricket::VideoCapturer* capturer) {
  if (!stream) {
    LOG(ERROR) << "AddNativeVideoMediaTrack called with null WebMediaStream.";
    return false;
  }

  // Create native track from the source.
  scoped_refptr<webrtc::VideoTrackInterface> native_track =
      CreateLocalVideoTrack(track_id, capturer);

  // Add the native track to native stream
  webrtc::MediaStreamInterface* native_stream =
      GetNativeMediaStream(*stream);
  DCHECK(native_stream);
  native_stream->AddTrack(native_track.get());

  // Create a new webkit video track.
  blink::WebMediaStreamTrack webkit_track;
  blink::WebMediaStreamSource webkit_source;
  blink::WebString webkit_track_id(UTF8ToUTF16(track_id));
  blink::WebMediaStreamSource::Type type =
      blink::WebMediaStreamSource::TypeVideo;
  webkit_source.initialize(webkit_track_id, type, webkit_track_id);

  webkit_track.initialize(webkit_track_id, webkit_source);
  AddNativeTrackToBlinkTrack(native_track.get(), webkit_track, true);

  // Add the track to WebMediaStream.
  stream->addTrack(webkit_track);
  return true;
}

bool MediaStreamDependencyFactory::RemoveNativeMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  webrtc::MediaStreamInterface* native_stream = extra_data->stream().get();
  DCHECK(native_stream);
  std::string track_id = UTF16ToUTF8(track.id());
  switch (track.source().type()) {
    case blink::WebMediaStreamSource::TypeAudio:
      return native_stream->RemoveTrack(
          native_stream->FindAudioTrack(track_id));
    case blink::WebMediaStreamSource::TypeVideo:
      return native_stream->RemoveTrack(
          native_stream->FindVideoTrack(track_id));
  }
  return false;
}

bool MediaStreamDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK(!pc_factory_.get());
  DCHECK(!audio_device_.get());
  DVLOG(1) << "MediaStreamDependencyFactory::CreatePeerConnectionFactory()";

  scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  scoped_refptr<RendererGpuVideoAcceleratorFactories> gpu_factories =
      RenderThreadImpl::current()->GetGpuFactories();
#if !defined(GOOGLE_TV)
  if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding)) {
    if (gpu_factories)
      decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));
  }
#else
  // PeerConnectionFactory will hold the ownership of this
  // VideoDecoderFactory.
  decoder_factory.reset(decoder_factory_tv_ = new RTCVideoDecoderFactoryTv());
#endif

  if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding)) {
    if (gpu_factories)
      encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecBridge::IsAvailable() ||
      !media::MediaCodecBridge::SupportsSetParameters()) {
    encoder_factory.reset();
  }
#endif

  scoped_refptr<WebRtcAudioDeviceImpl> audio_device(
      new WebRtcAudioDeviceImpl());

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(worker_thread_,
                                          signaling_thread_,
                                          audio_device.get(),
                                          encoder_factory.release(),
                                          decoder_factory.release()));
  if (!factory.get()) {
    return false;
  }

  audio_device_ = audio_device;
  pc_factory_ = factory;
  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.enable_aec_dump =
      cmd_line->HasSwitch(switches::kEnableWebRtcAecRecordings);
  factory_options.disable_sctp_data_channels =
      cmd_line->HasSwitch(switches::kDisableSCTPDataChannels);
  factory_options.disable_encryption =
      cmd_line->HasSwitch(switches::kDisableWebRtcEncryption);
  pc_factory_->SetOptions(factory_options);
  return true;
}

bool MediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

scoped_refptr<webrtc::PeerConnectionInterface>
MediaStreamDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::IceServers& ice_servers,
    const webrtc::MediaConstraintsInterface* constraints,
    blink::WebFrame* web_frame,
    webrtc::PeerConnectionObserver* observer) {
  CHECK(web_frame);
  CHECK(observer);

  scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            p2p_socket_dispatcher_.get(),
            network_manager_,
            socket_factory_.get(),
            web_frame);

  PeerConnectionIdentityService* identity_service =
      new PeerConnectionIdentityService(
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

scoped_refptr<WebAudioCapturerSource>
MediaStreamDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source,
    RTCMediaConstraints* constraints) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateWebAudioSource()";
  DCHECK(GetWebRtcAudioDevice());

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource());
  MediaStreamSourceExtraData* source_data = new MediaStreamSourceExtraData();

  // Create a LocalAudioSource object which holds audio options.
  // SetLocalAudioSource() affects core audio parts in third_party/Libjingle.
  source_data->SetLocalAudioSource(CreateLocalAudioSource(constraints).get());
  source->setExtraData(source_data);

  // Replace the default source with WebAudio as source instead.
  source->addAudioConsumer(webaudio_capturer_source.get());

  return webaudio_capturer_source;
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
    WebAudioCapturerSource* webaudio_source,
    webrtc::AudioSourceInterface* source,
    const webrtc::MediaConstraintsInterface* constraints) {
  // TODO(xians): Merge |source| to the capturer(). We can't do this today
  // because only one capturer() is supported while one |source| is created
  // for each audio track.
  scoped_refptr<WebRtcLocalAudioTrack> audio_track(
      WebRtcLocalAudioTrack::Create(id, capturer, webaudio_source,
                                    source, constraints));

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
  // TODO(xians): Handle the cases when gUM is called without a proper render
  // view, for example, by an extension.
  DCHECK_GE(render_view_id, 0);

  scoped_refptr<WebRtcAudioCapturer> capturer =
      GetWebRtcAudioDevice()->GetDefaultCapturer();

  // If the default capturer does not exist or |render_view_id| == -1, create
  // a new capturer.
  bool is_new_capturer = false;
  if (!capturer.get()) {
    capturer = WebRtcAudioCapturer::CreateCapturer();
    is_new_capturer = true;
  }

  if (!capturer->Initialize(
          render_view_id,
          static_cast<media::ChannelLayout>(
              device_info.device.input.channel_layout),
          device_info.device.input.sample_rate,
          device_info.device.input.frames_per_buffer,
          device_info.session_id,
          device_info.device.id,
          device_info.device.matched_output.sample_rate,
          device_info.device.matched_output.frames_per_buffer,
          device_info.device.input.effects)) {
    return NULL;
  }

  // Add the capturer to the WebRtcAudioDeviceImpl if it is a new capturer.
  if (is_new_capturer)
    GetWebRtcAudioDevice()->AddAudioCapturer(capturer);

  return capturer;
}

void MediaStreamDependencyFactory::AddNativeTrackToBlinkTrack(
    webrtc::MediaStreamTrackInterface* native_track,
    const blink::WebMediaStreamTrack& webkit_track,
    bool is_local_track) {
  DCHECK(!webkit_track.isNull() && !webkit_track.extraData());
  blink::WebMediaStreamTrack track = webkit_track;

  if (track.source().type() == blink::WebMediaStreamSource::TypeVideo) {
    track.setExtraData(new MediaStreamVideoTrack(
        static_cast<webrtc::VideoTrackInterface*>(native_track),
        is_local_track));
  } else {
    track.setExtraData(new MediaStreamTrackExtraData(native_track,
                                                     is_local_track));
  }
}

webrtc::MediaStreamInterface*
MediaStreamDependencyFactory::GetNativeMediaStream(
    const blink::WebMediaStream& stream) {
  if (stream.isNull())
    return NULL;
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  return extra_data ? extra_data->stream().get() : NULL;
}

webrtc::MediaStreamTrackInterface*
MediaStreamDependencyFactory::GetNativeMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) {
  if (track.isNull())
    return NULL;
  MediaStreamTrackExtraData* extra_data =
      static_cast<MediaStreamTrackExtraData*>(track.extraData());
  return extra_data ? extra_data->track().get() : NULL;
}

}  // namespace content
