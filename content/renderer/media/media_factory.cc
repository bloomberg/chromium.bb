// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_factory.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/buildflag.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_renderer_factory_impl.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/media/renderer_webmediaplayer_delegate.h"
#include "content/renderer/media/web_media_element_source_utils.h"
#include "content/renderer/media/webmediaplayer_ms.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/base/cdm_factory.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_switches.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/surface_manager.h"
#include "media/blink/webencryptedmediaclient_impl.h"
#include "media/blink/webmediaplayer_impl.h"
#include "media/filters/context_3d.h"
#include "media/media_features.h"
#include "media/renderers/default_renderer_factory.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

#if defined(OS_ANDROID)
#include "content/renderer/media/android/media_player_renderer_client_factory.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/renderer_surface_view_manager.h"
#include "content/renderer/media/android/stream_texture_wrapper_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/media.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA)
#include "content/renderer/media/media_interface_provider.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "media/mojo/clients/mojo_cdm_factory.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
#include "media/mojo/clients/mojo_renderer_factory.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) || BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/clients/mojo_decoder_factory.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "media/remoting/courier_renderer_factory.h"    // nogncheck
#include "media/remoting/remoting_cdm_controller.h"     // nogncheck
#include "media/remoting/remoting_cdm_factory.h"        // nogncheck
#include "media/remoting/renderer_controller.h"         // nogncheck
#include "media/remoting/shared_session.h"              // nogncheck
#include "media/remoting/sink_availability_observer.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/renderer/media/cdm/pepper_cdm_wrapper_impl.h"
#include "content/renderer/media/cdm/render_cdm_factory.h"
#endif

namespace content {

MediaFactory::MediaFactory(
    RenderFrameImpl* render_frame,
    media::RequestRoutingTokenCallback request_routing_token_cb)
    : render_frame_(render_frame),
      request_routing_token_cb_(std::move(request_routing_token_cb)) {}

MediaFactory::~MediaFactory() {}

void MediaFactory::SetupMojo() {
  // Only do setup once.
  DCHECK(!remote_interfaces_);

  remote_interfaces_ = render_frame_->GetRemoteInterfaces();
  DCHECK(remote_interfaces_);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  // Create the SinkAvailabilityObserver to monitor the remoting sink
  // availablity.
  media::mojom::RemotingSourcePtr remoting_source;
  auto remoting_source_request = mojo::MakeRequest(&remoting_source);
  media::mojom::RemoterPtr remoter;
  GetRemoterFactory()->Create(std::move(remoting_source),
                              mojo::MakeRequest(&remoter));
  remoting_sink_observer_ =
      base::MakeUnique<media::remoting::SinkAvailabilityObserver>(
          std::move(remoting_source_request), std::move(remoter));
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)
}

media::Context3D GetSharedMainThreadContext3D(
    scoped_refptr<ui::ContextProviderCommandBuffer> provider) {
  if (!provider)
    return media::Context3D();
  return media::Context3D(provider->ContextGL(), provider->GrContext());
}

#if defined(OS_ANDROID)
// Returns true if the MediaPlayerRenderer should be used for playback, false
// if the default renderer should be used instead.
//
// Note that HLS and MP4 detection are pre-redirect and path-based. It is
// possible to load such a URL and find different content.
bool UseMediaPlayerRenderer(const GURL& url) {
  // Always use the default renderer for playing blob URLs.
  if (url.SchemeIsBlob())
    return false;

  // The default renderer does not support HLS.
  if (media::MediaCodecUtil::IsHLSURL(url))
    return true;

  // Don't use the default renderer if the container likely contains a codec we
  // can't decode in software and platform decoders are not available.
  if (!media::HasPlatformDecoderSupport()) {
    // Assume that "mp4" means H264. Without platform decoder support we cannot
    // play it with the default renderer so use MediaPlayerRenderer.
    // http://crbug.com/642988.
    if (base::ToLowerASCII(url.spec()).find("mp4") != std::string::npos)
      return true;
  }

  // Indicates if the Android MediaPlayer should be used instead of WMPI.
  if (GetContentClient()->renderer()->ShouldUseMediaPlayerForURL(url))
    return true;

  // Otherwise, use the default renderer.
  return false;
}
#endif  // defined(OS_ANDROID)

blink::WebMediaPlayer* MediaFactory::CreateMediaPlayer(
    const blink::WebMediaPlayerSource& source,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm,
    const blink::WebString& sink_id) {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  blink::WebSecurityOrigin security_origin =
      render_frame_->GetWebFrame()->GetSecurityOrigin();
  blink::WebMediaStream web_stream =
      GetWebMediaStreamFromWebMediaPlayerSource(source);
  if (!web_stream.IsNull())
    return CreateWebMediaPlayerForMediaStream(client, sink_id, security_origin,
                                              web_frame);

  // If |source| was not a MediaStream, it must be a URL.
  // TODO(guidou): Fix this when support for other srcObject types is added.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // Render thread may not exist in tests, returning nullptr if it does not.
  if (!render_thread)
    return nullptr;

  scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink =
      AudioDeviceFactory::NewSwitchableAudioRendererSink(
          AudioDeviceFactory::kSourceMediaElement,
          render_frame_->GetRoutingID(), 0, sink_id.Utf8(), security_origin);
  // We need to keep a reference to the context provider (see crbug.com/610527)
  // but media/ can't depend on cc/, so for now, just keep a reference in the
  // callback.
  // TODO(piman): replace media::Context3D to scoped_refptr<ContextProvider> in
  // media/ once ContextProvider is in gpu/.
  media::WebMediaPlayerParams::Context3DCB context_3d_cb = base::Bind(
      &GetSharedMainThreadContext3D,
      RenderThreadImpl::current()->SharedMainThreadContextProvider());

  const WebPreferences webkit_preferences =
      render_frame_->GetWebkitPreferences();
  bool embedded_media_experience_enabled = false;
  bool use_media_player_renderer = false;
#if defined(OS_ANDROID)
  use_media_player_renderer = UseMediaPlayerRenderer(url);
  if (!use_media_player_renderer && !media_surface_manager_)
    media_surface_manager_ = new RendererSurfaceViewManager(render_frame_);
  embedded_media_experience_enabled =
      webkit_preferences.embedded_media_experience_enabled;
#endif  // defined(OS_ANDROID)

  // Enable background optimizations based on field trial for src= content, but
  // always enable for MSE content. See http://crbug.com/709302.
  base::TimeDelta max_keyframe_distance_to_disable_background_video =
      base::TimeDelta::FromMilliseconds(base::GetFieldTrialParamByFeatureAsInt(
          media::kBackgroundVideoTrackOptimization, "max_keyframe_distance_ms",
          0));
  base::TimeDelta max_keyframe_distance_to_disable_background_video_mse =
      base::TimeDelta::FromSeconds(5);

  // When memory pressure based garbage collection is enabled for MSE, the
  // |enable_instant_source_buffer_gc| flag controls whether the GC is done
  // immediately on memory pressure notification or during the next SourceBuffer
  // append (slower, but is MSE-spec compliant).
  bool enable_instant_source_buffer_gc =
      base::GetFieldTrialParamByFeatureAsBool(
          media::kMemoryPressureBasedSourceBufferGC,
          "enable_instant_source_buffer_gc", false);

  // This must be created for every new WebMediaPlayer, each instance generates
  // a new player id which is used to collate logs on the browser side.
  std::unique_ptr<media::MediaLog> media_log(
      new RenderMediaLog(url::Origin(security_origin).GetURL()));

  base::WeakPtr<media::MediaObserver> media_observer;

  auto factory_selector =
      CreateRendererFactorySelector(media_log.get(), use_media_player_renderer,
                                    GetDecoderFactory(), &media_observer);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  DCHECK(media_observer);
#endif

  if (!url_index_)
    url_index_ = base::MakeUnique<media::UrlIndex>(web_frame);
  DCHECK_EQ(url_index_->frame(), web_frame);

  std::unique_ptr<media::WebMediaPlayerParams> params(
      new media::WebMediaPlayerParams(
          std::move(media_log),
          base::Bind(&ContentRendererClient::DeferMediaLoad,
                     base::Unretained(GetContentClient()->renderer()),
                     static_cast<RenderFrame*>(render_frame_),
                     GetWebMediaPlayerDelegate()->has_played_media()),
          audio_renderer_sink, render_thread->GetMediaThreadTaskRunner(),
          render_thread->GetWorkerTaskRunner(),
          render_thread->compositor_task_runner(), context_3d_cb,
          base::Bind(&v8::Isolate::AdjustAmountOfExternalAllocatedMemory,
                     base::Unretained(blink::MainThreadIsolate())),
          initial_cdm, media_surface_manager_, request_routing_token_cb_,
          media_observer, max_keyframe_distance_to_disable_background_video,
          max_keyframe_distance_to_disable_background_video_mse,
          enable_instant_source_buffer_gc,
          embedded_media_experience_enabled));

  media::WebMediaPlayerImpl* media_player = new media::WebMediaPlayerImpl(
      web_frame, client, encrypted_client, GetWebMediaPlayerDelegate(),
      std::move(factory_selector), url_index_.get(), std::move(params));

#if defined(OS_ANDROID)  // WMPI_CAST
  media_player->SetMediaPlayerManager(GetMediaPlayerManager());
  media_player->SetDeviceScaleFactor(
      render_frame_->render_view()->GetDeviceScaleFactor());
#endif  // defined(OS_ANDROID)

  return media_player;
}

blink::WebEncryptedMediaClient* MediaFactory::EncryptedMediaClient() {
  if (!web_encrypted_media_client_) {
    web_encrypted_media_client_.reset(new media::WebEncryptedMediaClientImpl(
        GetCdmFactory(), render_frame_->GetMediaPermission(),
        new RenderMediaLog(
            url::Origin(render_frame_->GetWebFrame()->GetSecurityOrigin())
                .GetURL())));
  }
  return web_encrypted_media_client_.get();
}

std::unique_ptr<media::RendererFactorySelector>
MediaFactory::CreateRendererFactorySelector(
    media::MediaLog* media_log,
    bool use_media_player,
    media::DecoderFactory* decoder_factory,
    base::WeakPtr<media::MediaObserver>* out_media_observer) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // Render thread may not exist in tests, returning nullptr if it does not.
  if (!render_thread)
    return nullptr;

  auto factory_selector = base::MakeUnique<media::RendererFactorySelector>();

#if defined(OS_ANDROID)
  DCHECK(remote_interfaces_);

  // The only MojoRendererService that is registered at the RenderFrameHost
  // level uses the MediaPlayerRenderer as its underlying media::Renderer.
  auto mojo_media_player_renderer_factory =
      base::MakeUnique<media::MojoRendererFactory>(
          media::MojoRendererFactory::GetGpuFactoriesCB(),
          remote_interfaces_->get());

  // Always give |factory_selector| a MediaPlayerRendererClient factory. WMPI
  // might fallback to it if the final redirected URL is an HLS url.
  factory_selector->AddFactory(
      media::RendererFactorySelector::FactoryType::MEDIA_PLAYER,
      base::MakeUnique<MediaPlayerRendererClientFactory>(
          render_thread->compositor_task_runner(),
          std::move(mojo_media_player_renderer_factory),
          base::Bind(&StreamTextureWrapperImpl::Create,
                     render_thread->EnableStreamTextureCopy(),
                     render_thread->GetStreamTexureFactory(),
                     base::ThreadTaskRunnerHandle::Get())));

  factory_selector->SetUseMediaPlayer(use_media_player);
#endif  // defined(OS_ANDROID)

  bool use_mojo_renderer_factory = false;
#if BUILDFLAG(ENABLE_MOJO_RENDERER)
#if BUILDFLAG(ENABLE_RUNTIME_MEDIA_RENDERER_SELECTION)
  use_mojo_renderer_factory =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMojoRenderer);
#else
  use_mojo_renderer_factory = true;
#endif  // BUILDFLAG(ENABLE_RUNTIME_MEDIA_RENDERER_SELECTION)
  if (use_mojo_renderer_factory) {
    factory_selector->AddFactory(
        media::RendererFactorySelector::FactoryType::MOJO,
        base::MakeUnique<media::MojoRendererFactory>(
            base::Bind(&RenderThreadImpl::GetGpuFactories,
                       base::Unretained(render_thread)),
            GetMediaInterfaceProvider()));

    factory_selector->SetBaseFactoryType(
        media::RendererFactorySelector::FactoryType::MOJO);
  }
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

  if (!use_mojo_renderer_factory) {
    factory_selector->AddFactory(
        media::RendererFactorySelector::FactoryType::DEFAULT,
        base::MakeUnique<media::DefaultRendererFactory>(
            media_log, decoder_factory,
            base::Bind(&RenderThreadImpl::GetGpuFactories,
                       base::Unretained(render_thread))));

    factory_selector->SetBaseFactoryType(
        media::RendererFactorySelector::FactoryType::DEFAULT);
  }

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  media::mojom::RemotingSourcePtr remoting_source;
  auto remoting_source_request = mojo::MakeRequest(&remoting_source);
  media::mojom::RemoterPtr remoter;
  GetRemoterFactory()->Create(std::move(remoting_source),
                              mojo::MakeRequest(&remoter));
  using RemotingController = media::remoting::RendererController;
  std::unique_ptr<RemotingController> remoting_controller(
      new RemotingController(new media::remoting::SharedSession(
          std::move(remoting_source_request), std::move(remoter))));
  *out_media_observer = remoting_controller->GetWeakPtr();

  auto courier_factory =
      base::MakeUnique<media::remoting::CourierRendererFactory>(
          std::move(remoting_controller));

  // base::Unretained is safe here because |factory_selector| owns
  // |courier_factory|.
  factory_selector->SetQueryIsRemotingActiveCB(
      base::Bind(&media::remoting::CourierRendererFactory::IsRemotingActive,
                 base::Unretained(courier_factory.get())));

  factory_selector->AddFactory(
      media::RendererFactorySelector::FactoryType::COURIER,
      std::move(courier_factory));
#endif

  return factory_selector;
}

blink::WebMediaPlayer* MediaFactory::CreateWebMediaPlayerForMediaStream(
    blink::WebMediaPlayerClient* client,
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebLocalFrame* frame) {
#if BUILDFLAG(ENABLE_WEBRTC)
  RenderThreadImpl* const render_thread = RenderThreadImpl::current();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      render_thread->compositor_task_runner();
  if (!compositor_task_runner.get())
    compositor_task_runner = base::ThreadTaskRunnerHandle::Get();

  return new WebMediaPlayerMS(
      frame, client, GetWebMediaPlayerDelegate(),
      base::MakeUnique<RenderMediaLog>(url::Origin(security_origin).GetURL()),
      CreateMediaStreamRendererFactory(), render_thread->GetIOTaskRunner(),
      compositor_task_runner, render_thread->GetMediaThreadTaskRunner(),
      render_thread->GetWorkerTaskRunner(), render_thread->GetGpuFactories(),
      sink_id, security_origin);
#else
  return NULL;
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}

media::RendererWebMediaPlayerDelegate*
MediaFactory::GetWebMediaPlayerDelegate() {
  if (!media_player_delegate_) {
    media_player_delegate_ =
        new media::RendererWebMediaPlayerDelegate(render_frame_);
  }
  return media_player_delegate_;
}

std::unique_ptr<MediaStreamRendererFactory>
MediaFactory::CreateMediaStreamRendererFactory() {
  std::unique_ptr<MediaStreamRendererFactory> factory =
      GetContentClient()->renderer()->CreateMediaStreamRendererFactory();
  if (factory.get())
    return factory;
#if BUILDFLAG(ENABLE_WEBRTC)
  return std::unique_ptr<MediaStreamRendererFactory>(
      new MediaStreamRendererFactoryImpl());
#else
  return std::unique_ptr<MediaStreamRendererFactory>(
      static_cast<MediaStreamRendererFactory*>(NULL));
#endif
}

media::DecoderFactory* MediaFactory::GetDecoderFactory() {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) || BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  if (!decoder_factory_) {
    decoder_factory_.reset(
        new media::MojoDecoderFactory(GetMediaInterfaceProvider()));
  }
#endif
  return decoder_factory_.get();
}

#if defined(OS_ANDROID)
RendererMediaPlayerManager* MediaFactory::GetMediaPlayerManager() {
  if (!media_player_manager_)
    media_player_manager_ = new RendererMediaPlayerManager(render_frame_);
  return media_player_manager_;
}
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
media::mojom::RemoterFactory* MediaFactory::GetRemoterFactory() {
  if (!remoter_factory_) {
    DCHECK(remote_interfaces_);
    remote_interfaces_->GetInterface(&remoter_factory_);
  }
  return remoter_factory_.get();
}
#endif

media::CdmFactory* MediaFactory::GetCdmFactory() {
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  DCHECK(web_frame);

  if (cdm_factory_)
    return cdm_factory_.get();

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  static_assert(
      BUILDFLAG(ENABLE_MOJO_CDM),
      "Mojo CDM should always be enabled when library CDM is enabled");
  if (base::FeatureList::IsEnabled(media::kMojoCdm)) {
    cdm_factory_.reset(new media::MojoCdmFactory(GetMediaInterfaceProvider()));
  } else {
    cdm_factory_.reset(new RenderCdmFactory(
        base::Bind(&PepperCdmWrapperImpl::Create, web_frame)));
  }
#elif BUILDFLAG(ENABLE_MOJO_CDM)
  cdm_factory_.reset(new media::MojoCdmFactory(GetMediaInterfaceProvider()));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  cdm_factory_.reset(new media::remoting::RemotingCdmFactory(
      std::move(cdm_factory_), GetRemoterFactory(),
      std::move(remoting_sink_observer_)));
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

  return cdm_factory_.get();
}

#if BUILDFLAG(ENABLE_MOJO_MEDIA)
service_manager::mojom::InterfaceProvider*
MediaFactory::GetMediaInterfaceProvider() {
  if (!media_interface_provider_) {
    DCHECK(remote_interfaces_);
    media_interface_provider_.reset(
        new MediaInterfaceProvider(remote_interfaces_));
  }

  return media_interface_provider_.get();
}
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA)

}  // namespace content
