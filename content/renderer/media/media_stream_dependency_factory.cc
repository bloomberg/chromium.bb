// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dependency_factory.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_identity_service.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder_factory.h"
#include "content/renderer/media/rtc_video_encoder_factory.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
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

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_bridge.h"
#endif

namespace content {

// Map of corresponding media constraints and platform effects.
struct {
  const char* constraint;
  const media::AudioParameters::PlatformEffectsMask effect;
} const kConstraintEffectMap[] = {
  { content::kMediaStreamAudioDucking,
    media::AudioParameters::DUCKING },
  { webrtc::MediaConstraintsInterface::kEchoCancellation,
    media::AudioParameters::ECHO_CANCELLER },
};

// If any platform effects are available, check them against the constraints.
// Disable effects to match false constraints, but if a constraint is true, set
// the constraint to false to later disable the software effect.
//
// This function may modify both |constraints| and |effects|.
void HarmonizeConstraintsAndEffects(RTCMediaConstraints* constraints,
                                    int* effects) {
  if (*effects != media::AudioParameters::NO_EFFECTS) {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kConstraintEffectMap); ++i) {
      bool value;
      size_t is_mandatory = 0;
      if (!webrtc::FindConstraint(constraints,
                                  kConstraintEffectMap[i].constraint,
                                  &value,
                                  &is_mandatory) || !value) {
        // If the constraint is false, or does not exist, disable the platform
        // effect.
        *effects &= ~kConstraintEffectMap[i].effect;
        DVLOG(1) << "Disabling platform effect: "
                 << kConstraintEffectMap[i].effect;
      } else if (*effects & kConstraintEffectMap[i].effect) {
        // If the constraint is true, leave the platform effect enabled, and
        // set the constraint to false to later disable the software effect.
        if (is_mandatory) {
          constraints->AddMandatory(kConstraintEffectMap[i].constraint,
              webrtc::MediaConstraintsInterface::kValueFalse, true);
        } else {
          constraints->AddOptional(kConstraintEffectMap[i].constraint,
              webrtc::MediaConstraintsInterface::kValueFalse, true);
        }
        DVLOG(1) << "Disabling constraint: "
                 << kConstraintEffectMap[i].constraint;
      }
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

MediaStreamDependencyFactory::MediaStreamDependencyFactory(
    P2PSocketDispatcher* p2p_socket_dispatcher)
    : network_manager_(NULL),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread"),
      aec_dump_file_(base::kInvalidPlatformFileValue) {
}

MediaStreamDependencyFactory::~MediaStreamDependencyFactory() {
  CleanupPeerConnectionFactory();
  if (aec_dump_file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(aec_dump_file_);
}

blink::WebRTCPeerConnectionHandler*
MediaStreamDependencyFactory::CreateRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitRTCPeerConnection.
  UpdateWebRTCMethodCount(WEBKIT_RTC_PEER_CONNECTION);

  return new RTCPeerConnectionHandler(client, this);
}

bool MediaStreamDependencyFactory::InitializeMediaStreamAudioSource(
    int render_view_id,
    const blink::WebMediaConstraints& audio_constraints,
    MediaStreamAudioSource* source_data) {
  DVLOG(1) << "InitializeMediaStreamAudioSources()";

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  ApplyFixedAudioConstraints(&native_audio_constraints);

  StreamDeviceInfo device_info = source_data->device_info();
  RTCMediaConstraints constraints = native_audio_constraints;
  // May modify both |constraints| and |effects|.
  HarmonizeConstraintsAndEffects(&constraints,
                                 &device_info.device.input.effects);

  scoped_refptr<WebRtcAudioCapturer> capturer(
      CreateAudioCapturer(render_view_id, device_info, audio_constraints));
  if (!capturer.get()) {
    DLOG(WARNING) << "Failed to create the capturer for device "
        << device_info.device.id;
    // TODO(xians): Don't we need to check if source_observer is observing
    // something? If not, then it looks like we have a leak here.
    // OTOH, if it _is_ observing something, then the callback might
    // be called multiple times which is likely also a bug.
    return false;
  }
  source_data->SetAudioCapturer(capturer);

  // Creates a LocalAudioSource object which holds audio options.
  // TODO(xians): The option should apply to the track instead of the source.
  // TODO(perkj): Move audio constraints parsing to Chrome.
  // Currently there are a few constraints that are parsed by libjingle and
  // the state is set to ended if parsing fails.
  scoped_refptr<webrtc::AudioSourceInterface> rtc_source(
      CreateLocalAudioSource(&constraints).get());
  if (rtc_source->state() != webrtc::MediaSourceInterface::kLive) {
    DLOG(WARNING) << "Failed to create rtc LocalAudioSource.";
    return false;
  }
  source_data->SetLocalAudioSource(rtc_source);
  return true;
}

WebRtcVideoCapturerAdapter* MediaStreamDependencyFactory::CreateVideoCapturer(
    bool is_screeencast) {
  // We need to make sure the libjingle thread wrappers have been created
  // before we can use an instance of a WebRtcVideoCapturerAdapter. This is
  // since the base class of WebRtcVideoCapturerAdapter is a
  // cricket::VideoCapturer and it uses the libjingle thread wrappers.
  if (!GetPcFactory())
    return NULL;
  return new WebRtcVideoCapturerAdapter(is_screeencast);
}

scoped_refptr<webrtc::VideoSourceInterface>
MediaStreamDependencyFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const blink::WebMediaConstraints& constraints) {
  RTCMediaConstraints webrtc_constraints(constraints);
  scoped_refptr<webrtc::VideoSourceInterface> source =
      GetPcFactory()->CreateVideoSource(capturer, &webrtc_constraints).get();
  return source;
}

const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
MediaStreamDependencyFactory::GetPcFactory() {
  if (!pc_factory_)
    CreatePeerConnectionFactory();
  CHECK(pc_factory_);
  return pc_factory_;
}

void MediaStreamDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK(!pc_factory_.get());
  DCHECK(!signaling_thread_);
  DCHECK(!worker_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);
  DCHECK(!chrome_worker_thread_.IsRunning());

  DVLOG(1) << "MediaStreamDependencyFactory::CreatePeerConnectionFactory()";

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  signaling_thread_ = jingle_glue::JingleThreadWrapper::current();
  CHECK(signaling_thread_);

  CHECK(chrome_worker_thread_.Start());

  base::WaitableEvent start_worker_event(true, false);
  chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &MediaStreamDependencyFactory::InitializeWorkerThread,
      base::Unretained(this),
      &worker_thread_,
      &start_worker_event));
  start_worker_event.Wait();
  CHECK(worker_thread_);

  base::WaitableEvent create_network_manager_event(true, false);
  chrome_worker_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &MediaStreamDependencyFactory::CreateIpcNetworkManagerOnWorkerThread,
      base::Unretained(this),
      &create_network_manager_event));
  create_network_manager_event.Wait();

  socket_factory_.reset(
      new IpcPacketSocketFactory(p2p_socket_dispatcher_.get()));

  // Init SSL, which will be needed by PeerConnection.
#if defined(USE_OPENSSL)
  if (!talk_base::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }
#else
  // TODO(ronghuawu): Replace this call with InitializeSSL.
  net::EnsureNSSSSLInit();
#endif

  scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories =
      RenderThreadImpl::current()->GetGpuFactories();
  if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding)) {
    if (gpu_factories)
      decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));
  }

  if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding)) {
    if (gpu_factories)
      encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecBridge::SupportsSetParameters())
    encoder_factory.reset();
#endif

  EnsureWebRtcAudioDeviceImpl();

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(worker_thread_,
                                          signaling_thread_,
                                          audio_device_.get(),
                                          encoder_factory.release(),
                                          decoder_factory.release()));
  CHECK(factory);

  pc_factory_ = factory;
  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_sctp_data_channels = false;
  factory_options.disable_encryption =
      cmd_line->HasSwitch(switches::kDisableWebRtcEncryption);
  pc_factory_->SetOptions(factory_options);

  // |aec_dump_file| will be invalid when dump is not enabled.
  if (aec_dump_file_ != base::kInvalidPlatformFileValue) {
    StartAecDump(aec_dump_file_);
    aec_dump_file_ = base::kInvalidPlatformFileValue;
  }
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
  if (!GetPcFactory())
    return NULL;

  scoped_refptr<P2PPortAllocatorFactory> pa_factory =
        new talk_base::RefCountedObject<P2PPortAllocatorFactory>(
            p2p_socket_dispatcher_.get(),
            network_manager_,
            socket_factory_.get(),
            web_frame);

  PeerConnectionIdentityService* identity_service =
      new PeerConnectionIdentityService(
          GURL(web_frame->document().url().spec()).GetOrigin());

  return GetPcFactory()->CreatePeerConnection(ice_servers,
                                            constraints,
                                            pa_factory.get(),
                                            identity_service,
                                            observer).get();
}

scoped_refptr<webrtc::MediaStreamInterface>
MediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return GetPcFactory()->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::AudioSourceInterface>
MediaStreamDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  scoped_refptr<webrtc::AudioSourceInterface> source =
      GetPcFactory()->CreateAudioSource(constraints).get();
  return source;
}

void MediaStreamDependencyFactory::CreateLocalAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* source_data =
      static_cast<MediaStreamAudioSource*>(source.extraData());

  scoped_refptr<WebAudioCapturerSource> webaudio_source;
  if (!source_data) {
    if (source.requiresAudioConsumer()) {
      // We're adding a WebAudio MediaStream.
      // Create a specific capturer for each WebAudio consumer.
      webaudio_source = CreateWebAudioSource(&source);
      source_data =
          static_cast<MediaStreamAudioSource*>(source.extraData());
    } else {
      // TODO(perkj): Implement support for sources from
      // remote MediaStreams.
      NOTIMPLEMENTED();
      return;
    }
  }

  // Creates an adapter to hold all the libjingle objects.
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(track.id().utf8(),
                                           source_data->local_audio_source()));
  static_cast<webrtc::AudioTrackInterface*>(adapter.get())->set_enabled(
      track.isEnabled());

  // TODO(xians): Merge |source| to the capturer(). We can't do this today
  // because only one capturer() is supported while one |source| is created
  // for each audio track.
  scoped_ptr<WebRtcLocalAudioTrack> audio_track(
      new WebRtcLocalAudioTrack(adapter,
                                source_data->GetAudioCapturer(),
                                webaudio_source));

  StartLocalAudioTrack(audio_track.get());

  // Pass the ownership of the native local audio track to the blink track.
  blink::WebMediaStreamTrack writable_track = track;
  writable_track.setExtraData(audio_track.release());
}

void MediaStreamDependencyFactory::StartLocalAudioTrack(
    WebRtcLocalAudioTrack* audio_track) {
  // Add the WebRtcAudioDevice as the sink to the local audio track.
  // TODO(xians): Implement a PeerConnection sink adapter and remove this
  // AddSink() call.
  audio_track->AddSink(GetWebRtcAudioDevice());
  // Start the audio track. This will hook the |audio_track| to the capturer
  // as the sink of the audio, and only start the source of the capturer if
  // it is the first audio track connecting to the capturer.
  audio_track->Start();
}

scoped_refptr<WebAudioCapturerSource>
MediaStreamDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source) {
  DVLOG(1) << "MediaStreamDependencyFactory::CreateWebAudioSource()";

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource());
  MediaStreamAudioSource* source_data = new MediaStreamAudioSource();

  // Use the current default capturer for the WebAudio track so that the
  // WebAudio track can pass a valid delay value and |need_audio_processing|
  // flag to PeerConnection.
  // TODO(xians): Remove this after moving APM to Chrome.
  if (GetWebRtcAudioDevice()) {
    source_data->SetAudioCapturer(
        GetWebRtcAudioDevice()->GetDefaultCapturer());
  }

  // Create a LocalAudioSource object which holds audio options.
  // SetLocalAudioSource() affects core audio parts in third_party/Libjingle.
  source_data->SetLocalAudioSource(CreateLocalAudioSource(NULL).get());
  source->setExtraData(source_data);

  // Replace the default source with WebAudio as source instead.
  source->addAudioConsumer(webaudio_capturer_source.get());

  return webaudio_capturer_source;
}

scoped_refptr<webrtc::VideoTrackInterface>
MediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id, source).get();
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
      GetPcFactory()->CreateVideoSource(capturer, NULL).get();

  // Create native track from the source.
  return GetPcFactory()->CreateVideoTrack(id, source.get()).get();
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
MediaStreamDependencyFactory::CreateAudioCapturer(
    int render_view_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints) {
  // TODO(xians): Handle the cases when gUM is called without a proper render
  // view, for example, by an extension.
  DCHECK_GE(render_view_id, 0);

  EnsureWebRtcAudioDeviceImpl();
  DCHECK(GetWebRtcAudioDevice());
  return WebRtcAudioCapturer::CreateCapturer(render_view_id, device_info,
                                             constraints,
                                             GetWebRtcAudioDevice());
}

void MediaStreamDependencyFactory::AddNativeAudioTrackToBlinkTrack(
    webrtc::MediaStreamTrackInterface* native_track,
    const blink::WebMediaStreamTrack& webkit_track,
    bool is_local_track) {
  DCHECK(!webkit_track.isNull() && !webkit_track.extraData());
  DCHECK_EQ(blink::WebMediaStreamSource::TypeAudio,
            webkit_track.source().type());
  blink::WebMediaStreamTrack track = webkit_track;

  DVLOG(1) << "AddNativeTrackToBlinkTrack() audio";
  track.setExtraData(
      new MediaStreamTrack(
          static_cast<webrtc::AudioTrackInterface*>(native_track),
          is_local_track));
}

bool MediaStreamDependencyFactory::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDependencyFactory, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_EnableAecDump, OnAecDumpFile)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DisableAecDump, OnDisableAecDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDependencyFactory::OnAecDumpFile(
    IPC::PlatformFileForTransit file_handle) {
  DCHECK_EQ(aec_dump_file_, base::kInvalidPlatformFileValue);
  base::PlatformFile file =
      IPC::PlatformFileForTransitToPlatformFile(file_handle);
  DCHECK_NE(file, base::kInvalidPlatformFileValue);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAudioTrackProcessing)) {
    EnsureWebRtcAudioDeviceImpl();
    GetWebRtcAudioDevice()->EnableAecDump(file);
    return;
  }

  // TODO(xians): Remove the following code after kEnableAudioTrackProcessing
  // is removed.
  if (PeerConnectionFactoryCreated())
    StartAecDump(file);
  else
    aec_dump_file_ = file;
}

void MediaStreamDependencyFactory::OnDisableAecDump() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAudioTrackProcessing)) {
    GetWebRtcAudioDevice()->DisableAecDump();
    return;
  }

  // TODO(xians): Remove the following code after kEnableAudioTrackProcessing
  // is removed.
  if (aec_dump_file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(aec_dump_file_);
  aec_dump_file_ = base::kInvalidPlatformFileValue;
}

void MediaStreamDependencyFactory::StartAecDump(
    const base::PlatformFile& aec_dump_file) {
  // |pc_factory_| always takes ownership of |aec_dump_file|. If StartAecDump()
  // fails, |aec_dump_file| will be closed.
  if (!GetPcFactory()->StartAecDump(aec_dump_file))
    VLOG(1) << "Could not start AEC dump.";
}

void MediaStreamDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  if (audio_device_)
    return;

  audio_device_ = new WebRtcAudioDeviceImpl();
}

}  // namespace content
