// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_identity_store.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder_factory.h"
#include "content/renderer/media/rtc_video_encoder_factory.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/media_stream_remote_audio_track.h"
#include "content/renderer/media/webrtc/stun_field_trial.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/p2p/empty_network_manager.h"
#include "content/renderer/p2p/filtering_network_manager.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/p2p/port_allocator.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_features.h"
#include "crypto/openssl_util.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/media_permission.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/webrtc/base/ssladapter.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace content {

namespace {

enum WebRTCIPHandlingPolicy {
  DEFAULT,
  DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES,
  DEFAULT_PUBLIC_INTERFACE_ONLY,
  DISABLE_NON_PROXIED_UDP,
};

WebRTCIPHandlingPolicy GetWebRTCIPHandlingPolicy(
    const std::string& preference) {
  if (preference == kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces)
    return DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES;
  if (preference == kWebRTCIPHandlingDefaultPublicInterfaceOnly)
    return DEFAULT_PUBLIC_INTERFACE_ONLY;
  if (preference == kWebRTCIPHandlingDisableNonProxiedUdp)
    return DISABLE_NON_PROXIED_UDP;
  return DEFAULT;
}

}  // namespace

// Map of corresponding media constraints and platform effects.
struct {
  const char* constraint;
  const media::AudioParameters::PlatformEffectsMask effect;
} const kConstraintEffectMap[] = {
  { webrtc::MediaConstraintsInterface::kGoogEchoCancellation,
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
    for (size_t i = 0; i < arraysize(kConstraintEffectMap); ++i) {
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

PeerConnectionDependencyFactory::PeerConnectionDependencyFactory(
    P2PSocketDispatcher* p2p_socket_dispatcher)
    : network_manager_(NULL),
      p2p_socket_dispatcher_(p2p_socket_dispatcher),
      signaling_thread_(NULL),
      worker_thread_(NULL),
      chrome_signaling_thread_("Chrome_libJingle_Signaling"),
      chrome_worker_thread_("Chrome_libJingle_WorkerThread") {
  TryScheduleStunProbeTrial();
}

PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {
  DVLOG(1) << "~PeerConnectionDependencyFactory()";
  DCHECK(pc_factory_ == NULL);
}

blink::WebRTCPeerConnectionHandler*
PeerConnectionDependencyFactory::CreateRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  // Save histogram data so we can see how much PeerConnetion is used.
  // The histogram counts the number of calls to the JS API
  // webKitRTCPeerConnection.
  UpdateWebRTCMethodCount(WEBKIT_RTC_PEER_CONNECTION);

  return new RTCPeerConnectionHandler(client, this);
}

bool PeerConnectionDependencyFactory::InitializeMediaStreamAudioSource(
    int render_frame_id,
    const blink::WebMediaConstraints& audio_constraints,
    MediaStreamAudioSource* source_data) {
  DVLOG(1) << "InitializeMediaStreamAudioSources()";

  // Do additional source initialization if the audio source is a valid
  // microphone or tab audio.
  RTCMediaConstraints native_audio_constraints(audio_constraints);
  MediaAudioConstraints::ApplyFixedAudioConstraints(&native_audio_constraints);

  StreamDeviceInfo device_info = source_data->device_info();
  RTCMediaConstraints constraints = native_audio_constraints;
  // May modify both |constraints| and |effects|.
  HarmonizeConstraintsAndEffects(&constraints,
                                 &device_info.device.input.effects);

  scoped_refptr<WebRtcAudioCapturer> capturer(CreateAudioCapturer(
      render_frame_id, device_info, audio_constraints, source_data));
  if (!capturer.get()) {
    const std::string log_string =
        "PCDF::InitializeMediaStreamAudioSource: fails to create capturer";
    WebRtcLogMessage(log_string);
    DVLOG(1) << log_string;
    // TODO(xians): Don't we need to check if source_observer is observing
    // something? If not, then it looks like we have a leak here.
    // OTOH, if it _is_ observing something, then the callback might
    // be called multiple times which is likely also a bug.
    return false;
  }
  source_data->SetAudioCapturer(capturer.get());

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
  source_data->SetLocalAudioSource(rtc_source.get());
  return true;
}

WebRtcVideoCapturerAdapter*
PeerConnectionDependencyFactory::CreateVideoCapturer(
    bool is_screeencast) {
  // We need to make sure the libjingle thread wrappers have been created
  // before we can use an instance of a WebRtcVideoCapturerAdapter. This is
  // since the base class of WebRtcVideoCapturerAdapter is a
  // cricket::VideoCapturer and it uses the libjingle thread wrappers.
  if (!GetPcFactory().get())
    return NULL;
  return new WebRtcVideoCapturerAdapter(is_screeencast);
}

scoped_refptr<webrtc::VideoSourceInterface>
PeerConnectionDependencyFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const blink::WebMediaConstraints& constraints) {
  RTCMediaConstraints webrtc_constraints(constraints);
  scoped_refptr<webrtc::VideoSourceInterface> source =
      GetPcFactory()->CreateVideoSource(capturer, &webrtc_constraints).get();
  return source;
}

const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPcFactory() {
  if (!pc_factory_.get())
    CreatePeerConnectionFactory();
  CHECK(pc_factory_.get());
  return pc_factory_;
}

void PeerConnectionDependencyFactory::WillDestroyCurrentMessageLoop() {
  CleanupPeerConnectionFactory();
}

void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  DCHECK(!pc_factory_.get());
  DCHECK(!signaling_thread_);
  DCHECK(!worker_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);
  DCHECK(!chrome_signaling_thread_.IsRunning());
  DCHECK(!chrome_worker_thread_.IsRunning());

  DVLOG(1) << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";

#if BUILDFLAG(RTC_USE_H264)
  // TODO(hbos): This is temporary. Disable the runtime effects of building with
  // |rtc_use_h264|. We are planning to default the |rtc_use_h264| flag to
  // |proprietary_codecs| so that it will be used by Chromium trybots. This
  // would also make it used by Chrome, but this feature is not ready to be
  // launched yet. An upcoming CL will add browser tests for H264. That CL will
  // remove this line. It should remain disabled until tested.
  webrtc::DisableRtcUseH264();
  // When building with |rtc_use_h264|, |H264DecoderImpl| may be used which
  // depends on FFmpeg, therefore we need to initialize FFmpeg before going
  // further.
  // TODO(hbos): Temporarily commented out due to webrtc::DisableRtcUseH264(),
  // no need to initialize FFmpeg when |H264DecoderImpl| is disabled.
  // media::FFmpegGlue::InitializeFFmpeg();
#endif

  base::MessageLoop::current()->AddDestructionObserver(this);
  // To allow sending to the signaling/worker threads.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  CHECK(chrome_signaling_thread_.Start());
  CHECK(chrome_worker_thread_.Start());

  base::WaitableEvent start_worker_event(true, false);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::InitializeWorkerThread,
                 base::Unretained(this), &worker_thread_, &start_worker_event));

  base::WaitableEvent create_network_manager_event(true, false);
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::
                     CreateIpcNetworkManagerOnWorkerThread,
                 base::Unretained(this), &create_network_manager_event));

  start_worker_event.Wait();
  create_network_manager_event.Wait();

  CHECK(worker_thread_);

  // Init SSL, which will be needed by PeerConnection.
  //
  // TODO(davidben): BoringSSL must be initialized by Chromium code. If the
  // initialization requirement is removed or when different libraries are
  // allowed to call CRYPTO_library_init concurrently, remove this line and
  // initialize within WebRTC. See https://crbug.com/542879.
  crypto::EnsureOpenSSLInit();
  if (!rtc::InitializeSSL()) {
    LOG(ERROR) << "Failed on InitializeSSL.";
    NOTREACHED();
    return;
  }

  base::WaitableEvent start_signaling_event(true, false);
  chrome_signaling_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&PeerConnectionDependencyFactory::InitializeSignalingThread,
                 base::Unretained(this),
                 RenderThreadImpl::current()->GetGpuFactories(),
                 &start_signaling_event));

  start_signaling_event.Wait();
  CHECK(signaling_thread_);
}

void PeerConnectionDependencyFactory::InitializeSignalingThread(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    base::WaitableEvent* event) {
  DCHECK(chrome_signaling_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(worker_thread_);
  DCHECK(p2p_socket_dispatcher_.get());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  signaling_thread_ = jingle_glue::JingleThreadWrapper::current();

  EnsureWebRtcAudioDeviceImpl();

  socket_factory_.reset(
      new IpcPacketSocketFactory(p2p_socket_dispatcher_.get()));

  scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (gpu_factories && gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWDecoding))
      decoder_factory.reset(new RTCVideoDecoderFactory(gpu_factories));

    if (!cmd_line->HasSwitch(switches::kDisableWebRtcHWEncoding))
      encoder_factory.reset(new RTCVideoEncoderFactory(gpu_factories));
  }

#if defined(OS_ANDROID)
  if (!media::MediaCodecUtil::SupportsSetParameters())
    encoder_factory.reset();
#endif

  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      worker_thread_, signaling_thread_, audio_device_.get(),
      encoder_factory.release(), decoder_factory.release());
  CHECK(pc_factory_.get());

  webrtc::PeerConnectionFactoryInterface::Options factory_options;
  factory_options.disable_sctp_data_channels = false;
  factory_options.disable_encryption =
      cmd_line->HasSwitch(switches::kDisableWebRtcEncryption);

  // DTLS 1.2 is the default now but could be changed to 1.0 by the experiment.
  factory_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  std::string group_name =
      base::FieldTrialList::FindFullName("WebRTC-PeerConnectionDTLS1.2");
  if (StartsWith(group_name, "Control", base::CompareCase::SENSITIVE))
    factory_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;

  pc_factory_->SetOptions(factory_options);

  event->Signal();
}

bool PeerConnectionDependencyFactory::PeerConnectionFactoryCreated() {
  return pc_factory_.get() != NULL;
}

scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const webrtc::MediaConstraintsInterface* constraints,
    blink::WebFrame* web_frame,
    webrtc::PeerConnectionObserver* observer) {
  CHECK(web_frame);
  CHECK(observer);
  if (!GetPcFactory().get())
    return NULL;

  rtc::scoped_ptr<PeerConnectionIdentityStore> identity_store(
      new PeerConnectionIdentityStore(
          base::ThreadTaskRunnerHandle::Get(),
          GetWebRtcSignalingThread(),
          GURL(web_frame->document().url()),
          GURL(web_frame->document().firstPartyForCookies())));

  // Copy the flag from Preference associated with this WebFrame.
  P2PPortAllocator::Config port_config;

  // |media_permission| will be called to check mic/camera permission. If at
  // least one of them is granted, P2PPortAllocator is allowed to gather local
  // host IP addresses as ICE candidates. |media_permission| could be nullptr,
  // which means the permission will be granted automatically. This could be the
  // case when either the experiment is not enabled or the preference is not
  // enforced.
  //
  // Note on |media_permission| lifetime: |media_permission| is owned by a frame
  // (RenderFrameImpl). It is also stored as an indirect member of
  // RTCPeerConnectionHandler (through PeerConnection/PeerConnectionInterface ->
  // P2PPortAllocator -> FilteringNetworkManager -> |media_permission|).
  // The RTCPeerConnectionHandler is owned as RTCPeerConnection::m_peerHandler
  // in Blink, which will be reset in RTCPeerConnection::stop(). Since
  // ActiveDOMObject::stop() is guaranteed to be called before a frame is
  // detached, it is impossible for RTCPeerConnectionHandler to outlive the
  // frame. Therefore using a raw pointer of |media_permission| is safe here.
  media::MediaPermission* media_permission = nullptr;
  if (!GetContentClient()
           ->renderer()
           ->ShouldEnforceWebRTCRoutingPreferences()) {
    port_config.enable_multiple_routes = true;
    port_config.enable_nonproxied_udp = true;
    VLOG(3) << "WebRTC routing preferences will not be enforced";
  } else {
    if (web_frame && web_frame->view()) {
      RenderViewImpl* renderer_view_impl =
          RenderViewImpl::FromWebView(web_frame->view());
      if (renderer_view_impl) {
        // TODO(guoweis): |enable_multiple_routes| should be renamed to
        // |request_multiple_routes|. Whether local IP addresses could be
        // collected depends on if mic/camera permission is granted for this
        // origin.
        WebRTCIPHandlingPolicy policy =
            GetWebRTCIPHandlingPolicy(renderer_view_impl->renderer_preferences()
                                          .webrtc_ip_handling_policy);
        switch (policy) {
          // TODO(guoweis): specify the flag of disabling local candidate
          // collection when webrtc is updated.
          case DEFAULT_PUBLIC_INTERFACE_ONLY:
          case DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES:
            port_config.enable_multiple_routes = false;
            port_config.enable_nonproxied_udp = true;
            port_config.enable_default_local_candidate =
                (policy == DEFAULT_PUBLIC_AND_PRIVATE_INTERFACES);
            break;
          case DISABLE_NON_PROXIED_UDP:
            port_config.enable_multiple_routes = false;
            port_config.enable_nonproxied_udp = false;
            break;
          case DEFAULT:
            port_config.enable_multiple_routes = true;
            port_config.enable_nonproxied_udp = true;
            break;
        }

        VLOG(3) << "WebRTC routing preferences: "
                << "policy: " << policy
                << ", multiple_routes: " << port_config.enable_multiple_routes
                << ", nonproxied_udp: " << port_config.enable_nonproxied_udp;
      }
    }
    if (port_config.enable_multiple_routes) {
      bool create_media_permission =
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnforceWebRtcIPPermissionCheck);
      create_media_permission =
          create_media_permission ||
          StartsWith(base::FieldTrialList::FindFullName(
                         "WebRTC-LocalIPPermissionCheck"),
                     "Enabled", base::CompareCase::SENSITIVE);
      if (create_media_permission) {
        content::RenderFrameImpl* render_frame =
            content::RenderFrameImpl::FromWebFrame(web_frame);
        if (render_frame)
          media_permission = render_frame->GetMediaPermission();
        DCHECK(media_permission);
      }
    }
  }

  const GURL& requesting_origin =
      GURL(web_frame->document().url()).GetOrigin();

  scoped_ptr<rtc::NetworkManager> network_manager;
  if (port_config.enable_multiple_routes) {
    FilteringNetworkManager* filtering_network_manager =
        new FilteringNetworkManager(network_manager_, requesting_origin,
                                    media_permission);
    if (media_permission) {
      // Start permission check earlier to reduce any impact to call set up
      // time. It's safe to use Unretained here since both destructor and
      // Initialize can only be called on the worker thread.
      chrome_worker_thread_.task_runner()->PostTask(
          FROM_HERE, base::Bind(&FilteringNetworkManager::Initialize,
                                base::Unretained(filtering_network_manager)));
    }
    network_manager.reset(filtering_network_manager);
  } else {
    network_manager.reset(new EmptyNetworkManager(network_manager_));
  }
  rtc::scoped_ptr<P2PPortAllocator> port_allocator(new P2PPortAllocator(
      p2p_socket_dispatcher_, std::move(network_manager), socket_factory_.get(),
      port_config, requesting_origin, chrome_worker_thread_.task_runner()));

  return GetPcFactory()
      ->CreatePeerConnection(config, constraints, std::move(port_allocator),
                             std::move(identity_store), observer)
      .get();
}

scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  return GetPcFactory()->CreateLocalMediaStream(label).get();
}

scoped_refptr<webrtc::AudioSourceInterface>
PeerConnectionDependencyFactory::CreateLocalAudioSource(
    const webrtc::MediaConstraintsInterface* constraints) {
  scoped_refptr<webrtc::AudioSourceInterface> source =
      GetPcFactory()->CreateAudioSource(constraints).get();
  return source;
}

void PeerConnectionDependencyFactory::CreateLocalAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  DCHECK(!source.remote());
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
      NOTREACHED() << "Local track missing source extra data.";
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
  scoped_ptr<WebRtcLocalAudioTrack> audio_track(new WebRtcLocalAudioTrack(
      adapter.get(), source_data->GetAudioCapturer(), webaudio_source.get()));

  StartLocalAudioTrack(audio_track.get());

  // Pass the ownership of the native local audio track to the blink track.
  blink::WebMediaStreamTrack writable_track = track;
  writable_track.setExtraData(audio_track.release());
}

void PeerConnectionDependencyFactory::CreateRemoteAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  DCHECK(source.remote());
  DCHECK(source.extraData());

  blink::WebMediaStreamTrack writable_track = track;
  writable_track.setExtraData(
      new MediaStreamRemoteAudioTrack(source, track.isEnabled()));
}

void PeerConnectionDependencyFactory::StartLocalAudioTrack(
    WebRtcLocalAudioTrack* audio_track) {
  // Start the audio track. This will hook the |audio_track| to the capturer
  // as the sink of the audio, and only start the source of the capturer if
  // it is the first audio track connecting to the capturer.
  audio_track->Start();
}

scoped_refptr<WebAudioCapturerSource>
PeerConnectionDependencyFactory::CreateWebAudioSource(
    blink::WebMediaStreamSource* source) {
  DVLOG(1) << "PeerConnectionDependencyFactory::CreateWebAudioSource()";

  scoped_refptr<WebAudioCapturerSource>
      webaudio_capturer_source(new WebAudioCapturerSource(*source));
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
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoSourceInterface* source) {
  return GetPcFactory()->CreateVideoTrack(id, source).get();
}

scoped_refptr<webrtc::VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
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
PeerConnectionDependencyFactory::CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    webrtc::SdpParseError* error) {
  return webrtc::CreateSessionDescription(type, sdp, error);
}

webrtc::IceCandidateInterface*
PeerConnectionDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, nullptr);
}

bool PeerConnectionDependencyFactory::StartRtcEventLog(
    base::PlatformFile file) {
  return GetPcFactory()->StartRtcEventLog(file);
}

void PeerConnectionDependencyFactory::StopRtcEventLog() {
  GetPcFactory()->StopRtcEventLog();
}

WebRtcAudioDeviceImpl*
PeerConnectionDependencyFactory::GetWebRtcAudioDevice() {
  return audio_device_.get();
}

void PeerConnectionDependencyFactory::InitializeWorkerThread(
    rtc::Thread** thread,
    base::WaitableEvent* event) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = jingle_glue::JingleThreadWrapper::current();
  event->Signal();
}

void PeerConnectionDependencyFactory::TryScheduleStunProbeTrial() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (!cmd_line->HasSwitch(switches::kWebRtcStunProbeTrialParameter))
    return;

  // The underneath IPC channel has to be connected before sending any IPC
  // message.
  if (!p2p_socket_dispatcher_->connected()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PeerConnectionDependencyFactory::TryScheduleStunProbeTrial,
                   base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
    return;
  }

  // GetPcFactory could trigger an IPC message. If done before
  // |p2p_socket_dispatcher_| is connected, that'll put the
  // |p2p_socket_dispatcher_| in a bad state such that no other IPC message can
  // be processed.
  GetPcFactory();

  const std::string params =
      cmd_line->GetSwitchValueASCII(switches::kWebRtcStunProbeTrialParameter);

  chrome_worker_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread,
          base::Unretained(this), params),
      base::TimeDelta::FromMilliseconds(kExperimentStartDelayMs));
}

void PeerConnectionDependencyFactory::StartStunProbeTrialOnWorkerThread(
    const std::string& params) {
  DCHECK(network_manager_);
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  stun_trial_.reset(
      new StunProberTrial(network_manager_, params, socket_factory_.get()));
}

void PeerConnectionDependencyFactory::CreateIpcNetworkManagerOnWorkerThread(
    base::WaitableEvent* event) {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  network_manager_ = new IpcNetworkManager(p2p_socket_dispatcher_.get());
  event->Signal();
}

void PeerConnectionDependencyFactory::DeleteIpcNetworkManager() {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  delete network_manager_;
  network_manager_ = NULL;
}

void PeerConnectionDependencyFactory::CleanupPeerConnectionFactory() {
  DVLOG(1) << "PeerConnectionDependencyFactory::CleanupPeerConnectionFactory()";
  pc_factory_ = NULL;
  if (network_manager_) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the worked thread.
    if (chrome_worker_thread_.IsRunning()) {
      chrome_worker_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&PeerConnectionDependencyFactory::DeleteIpcNetworkManager,
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
PeerConnectionDependencyFactory::CreateAudioCapturer(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    MediaStreamAudioSource* audio_source) {
  // TODO(xians): Handle the cases when gUM is called without a proper render
  // view, for example, by an extension.
  DCHECK_GE(render_frame_id, 0);

  EnsureWebRtcAudioDeviceImpl();
  DCHECK(GetWebRtcAudioDevice());
  return WebRtcAudioCapturer::CreateCapturer(
      render_frame_id, device_info, constraints, GetWebRtcAudioDevice(),
      audio_source);
}

void PeerConnectionDependencyFactory::EnsureInitialized() {
  DCHECK(CalledOnValidThread());
  GetPcFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcWorkerThread() const {
  DCHECK(CalledOnValidThread());
  return chrome_worker_thread_.IsRunning() ? chrome_worker_thread_.task_runner()
                                           : nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
PeerConnectionDependencyFactory::GetWebRtcSignalingThread() const {
  DCHECK(CalledOnValidThread());
  return chrome_signaling_thread_.IsRunning()
             ? chrome_signaling_thread_.task_runner()
             : nullptr;
}

void PeerConnectionDependencyFactory::EnsureWebRtcAudioDeviceImpl() {
  if (audio_device_.get())
    return;

  audio_device_ = new WebRtcAudioDeviceImpl();
}

}  // namespace content
