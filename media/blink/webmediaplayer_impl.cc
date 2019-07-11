// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/layers/video_layer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/encryption_scheme.h"
#include "media/base/limits.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/text_renderer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/blink/texttrack_impl.h"
#include "media/blink/url_index.h"
#include "media/blink/video_decode_stats_reporter.h"
#include "media/blink/watch_time_reporter.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webinbandtexttrack_impl.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/blink/webmediasource_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/memory_data_source.h"
#include "media/media_buildflags.h"
#include "net/base/data_url.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_localized_string.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using gpu::gles2::GLES2Interface;

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace media {

namespace {

void SetSinkIdOnMediaThread(
    scoped_refptr<blink::WebAudioSourceProviderImpl> sink,
    const std::string& device_id,
    OutputDeviceStatusCB callback) {
  sink->SwitchOutputDevice(device_id, std::move(callback));
}

bool IsBackgroundSuspendEnabled(const WebMediaPlayerImpl* wmpi) {
  // TODO(crbug.com/867146): remove these switches.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaSuspend))
    return false;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMediaSuspend))
    return true;

  return wmpi->IsBackgroundMediaSuspendEnabled();
}

bool IsResumeBackgroundVideosEnabled() {
  return base::FeatureList::IsEnabled(kResumeBackgroundVideo);
}

bool IsBackgroundVideoPauseOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundVideoPauseOptimization);
}

bool IsNetworkStateError(blink::WebMediaPlayer::NetworkState state) {
  bool result = state == blink::WebMediaPlayer::kNetworkStateFormatError ||
                state == blink::WebMediaPlayer::kNetworkStateNetworkError ||
                state == blink::WebMediaPlayer::kNetworkStateDecodeError;
  DCHECK_EQ(state > blink::WebMediaPlayer::kNetworkStateLoaded, result);
  return result;
}

gfx::Size GetRotatedVideoSize(VideoRotation rotation, gfx::Size natural_size) {
  if (rotation == VIDEO_ROTATION_90 || rotation == VIDEO_ROTATION_270)
    return gfx::Size(natural_size.height(), natural_size.width());
  return natural_size;
}

void RecordEncryptedEvent(bool encrypted_event_fired) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.EncryptedEvent", encrypted_event_fired);
}

// How much time must have elapsed since loading last progressed before we
// assume that the decoder will have had time to complete preroll.
constexpr base::TimeDelta kPrerollAttemptTimeout =
    base::TimeDelta::FromSeconds(3);

// Maximum number, per-WMPI, of media logs of playback rate changes.
constexpr int kMaxNumPlaybackRateLogs = 10;

blink::WebLocalizedString::Name GetSwitchToLocalMessage(
    MediaObserverClient::ReasonToSwitchToLocal reason) {
  switch (reason) {
    case MediaObserverClient::ReasonToSwitchToLocal::NORMAL:
      return blink::WebLocalizedString::kMediaRemotingStopText;
    case MediaObserverClient::ReasonToSwitchToLocal::POOR_PLAYBACK_QUALITY:
      return blink::WebLocalizedString::kMediaRemotingStopByPlaybackQualityText;
    case MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR:
      return blink::WebLocalizedString::kMediaRemotingStopByErrorText;
    case MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED:
      return blink::WebLocalizedString::kMediaRemotingStopNoText;
  }
  NOTREACHED();
  // To suppress compiler warning on Windows.
  return blink::WebLocalizedString::kMediaRemotingStopNoText;
}

// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/825041): This should use EncryptionMode when kUnencrypted
// removed.
enum class EncryptionSchemeUMA { kCenc = 0, kCbcs = 1, kCount };

EncryptionSchemeUMA DetermineEncryptionSchemeUMAValue(
    const EncryptionScheme& encryption_scheme) {
  if (encryption_scheme.mode() == EncryptionScheme::CIPHER_MODE_AES_CBC)
    return EncryptionSchemeUMA::kCbcs;

  DCHECK_EQ(encryption_scheme.mode(), EncryptionScheme::CIPHER_MODE_AES_CTR);
  return EncryptionSchemeUMA::kCenc;
}

EncryptionMode DetermineEncryptionMode(
    const EncryptionScheme& encryption_scheme) {
  switch (encryption_scheme.mode()) {
    case EncryptionScheme::CIPHER_MODE_UNENCRYPTED:
      return EncryptionMode::kUnencrypted;
    case EncryptionScheme::CIPHER_MODE_AES_CTR:
      return EncryptionMode::kCenc;
    case EncryptionScheme::CIPHER_MODE_AES_CBC:
      return EncryptionMode::kCbcs;
  }
}

#if BUILDFLAG(ENABLE_FFMPEG)
// Returns true if |url| represents (or is likely to) a local file.
bool IsLocalFile(const GURL& url) {
  return url.SchemeIsFile() || url.SchemeIsFileSystem() ||
         url.SchemeIs(url::kContentScheme) ||
         url.SchemeIs(url::kContentIDScheme) ||
         url.SchemeIs("chrome-extension");
}
#endif

// Handles destruction of media::Renderer dependent components after the
// renderer has been destructed on the media thread.
void DestructionHelper(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> vfc_task_runner,
    std::unique_ptr<Demuxer> demuxer,
    std::unique_ptr<DataSource> data_source,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<CdmContextRef> cdm_context_1,
    std::unique_ptr<CdmContextRef> cdm_context_2,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
    std::unique_ptr<blink::WebSurfaceLayerBridge> bridge,
    bool is_chunk_demuxer) {
  // We release |bridge| after pipeline stop to ensure layout tests receive
  // painted video frames before test harness exit.
  main_task_runner->DeleteSoon(FROM_HERE, std::move(bridge));

  // Since the media::Renderer is gone we can now destroy the compositor and
  // renderer factory selector.
  vfc_task_runner->DeleteSoon(FROM_HERE, std::move(compositor));
  main_task_runner->DeleteSoon(FROM_HERE, std::move(renderer_factory_selector));

  // ChunkDemuxer can be deleted on any thread, but other demuxers are bound to
  // the main thread and must be deleted there now that the renderer is gone.
  if (!is_chunk_demuxer) {
    main_task_runner->DeleteSoon(FROM_HERE, std::move(demuxer));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(data_source));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_1));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_2));
    main_task_runner->DeleteSoon(FROM_HERE, std::move(media_log));
    return;
  }

  // ChunkDemuxer's streams may contain much buffered, compressed media that
  // may need to be paged back in during destruction.  Paging delay may exceed
  // the renderer hang monitor's threshold on at least Windows while also
  // blocking other work on the renderer main thread, so we do the actual
  // destruction in the background without blocking WMPI destruction or
  // |task_runner|.  On advice of task_scheduler OWNERS, MayBlock() is not
  // used because virtual memory overhead is not considered blocking I/O; and
  // CONTINUE_ON_SHUTDOWN is used to allow process termination to not block on
  // completing the task.
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
             std::unique_ptr<Demuxer> demuxer_to_destroy,
             std::unique_ptr<CdmContextRef> cdm_context_1,
             std::unique_ptr<CdmContextRef> cdm_context_2,
             std::unique_ptr<MediaLog> media_log) {
            SCOPED_UMA_HISTOGRAM_TIMER("Media.MSE.DemuxerDestructionTime");
            demuxer_to_destroy.reset();
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_1));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(cdm_context_2));
            main_task_runner->DeleteSoon(FROM_HERE, std::move(media_log));
          },
          std::move(main_task_runner), std::move(demuxer),
          std::move(cdm_context_1), std::move(cdm_context_2),
          std::move(media_log)));
}

}  // namespace

class BufferedDataSourceHostImpl;

STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUnspecified,
                   UrlData::CORS_UNSPECIFIED);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeAnonymous, UrlData::CORS_ANONYMOUS);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCorsModeUseCredentials,
                   UrlData::CORS_USE_CREDENTIALS);

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
    UrlIndex* url_index,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<WebMediaPlayerParams> params)
    : frame_(frame),
      main_task_runner_(
          frame->GetTaskRunner(blink::TaskType::kMediaElementEvent)),
      media_task_runner_(params->media_task_runner()),
      worker_task_runner_(params->worker_task_runner()),
      media_log_(params->take_media_log()),
      client_(client),
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      defer_load_cb_(params->defer_load_cb()),
      adjust_allocated_memory_cb_(params->adjust_allocated_memory_cb()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      url_index_(url_index),
      context_provider_(params->context_provider()),
      vfc_task_runner_(params->video_frame_compositor_task_runner()),
      compositor_(std::move(compositor)),
      renderer_factory_selector_(std::move(renderer_factory_selector)),
      observer_(params->media_observer()),
      enable_instant_source_buffer_gc_(
          params->enable_instant_source_buffer_gc()),
      embedded_media_experience_enabled_(
          params->embedded_media_experience_enabled()),
      surface_layer_mode_(params->use_surface_layer_for_video()),
      create_bridge_callback_(params->create_bridge_callback()),
      request_routing_token_cb_(params->request_routing_token_cb()),
      overlay_routing_token_(OverlayInfo::RoutingToken()),
      media_metrics_provider_(params->take_metrics_provider()),
      is_background_suspend_enabled_(params->IsBackgroundSuspendEnabled()),
      is_background_video_playback_enabled_(
          params->IsBackgroundVideoPlaybackEnabled()),
      is_background_video_track_optimization_supported_(
          params->IsBackgroundVideoTrackOptimizationSupported()) {
  DVLOG(1) << __func__;
  DCHECK(adjust_allocated_memory_cb_);
  DCHECK(renderer_factory_selector_);
  DCHECK(client_);
  DCHECK(delegate_);

  weak_this_ = weak_factory_.GetWeakPtr();

  pipeline_controller_ = std::make_unique<PipelineController>(
      std::make_unique<PipelineImpl>(media_task_runner_, main_task_runner_,
                                     media_log_.get()),
      base::BindRepeating(&WebMediaPlayerImpl::CreateRenderer,
                          base::Unretained(this)),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSeeked, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineSuspended, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnBeforePipelineResume,
                          weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnPipelineResumed, weak_this_),
      base::BindRepeating(&WebMediaPlayerImpl::OnError, weak_this_));

  buffered_data_source_host_ = std::make_unique<BufferedDataSourceHostImpl>(
      base::BindRepeating(&WebMediaPlayerImpl::OnProgress, weak_this_),
      tick_clock_);

  // If we're supposed to force video overlays, then make sure that they're
  // enabled all the time.
  always_enable_overlays_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceVideoOverlays);

  if (base::FeatureList::IsEnabled(kOverlayFullscreenVideo)) {
    bool use_android_overlay = base::FeatureList::IsEnabled(kUseAndroidOverlay);
    overlay_mode_ = use_android_overlay ? OverlayMode::kUseAndroidOverlay
                                        : OverlayMode::kUseContentVideoView;
  } else {
    overlay_mode_ = OverlayMode::kNoOverlays;
  }

  delegate_id_ = delegate_->AddObserver(this);
  delegate_->SetIdle(delegate_id_, true);

  media_log_->AddEvent(media_log_->CreateCreatedEvent(
      url::Origin(frame_->GetSecurityOrigin()).GetURL().spec()));
  media_log_->SetStringProperty("frame_url",
                                frame_->GetDocument().Url().GetString().Utf8());
  media_log_->SetStringProperty("frame_title",
                                frame_->GetDocument().Title().Utf8());

  // To make manual testing easier, include |surface_layer_mode_| in the log.
  // TODO(liberato): Move this into media_factory.cc, so that it can be shared
  // with the MediaStream startup.
  const char* surface_layer_mode_name = "(unset)";
  switch (surface_layer_mode_) {
    case SurfaceLayerMode::kAlways:
      surface_layer_mode_name = "kAlways";
      break;
    case SurfaceLayerMode::kOnDemand:
      surface_layer_mode_name = "kOnDemand";
      break;
    case SurfaceLayerMode::kNever:
      surface_layer_mode_name = "kNever";
      break;
  }
  media_log_->SetStringProperty("surface_layer_mode", surface_layer_mode_name);

  if (params->initial_cdm())
    SetCdmInternal(params->initial_cdm());

  // Report a false "EncrytpedEvent" here as a baseline.
  RecordEncryptedEvent(false);

  // TODO(xhwang): When we use an external Renderer, many methods won't work,
  // e.g. GetCurrentFrameFromCompositor(). See http://crbug.com/434861
  audio_source_provider_ = new blink::WebAudioSourceProviderImpl(
      params->audio_renderer_sink(), media_log_.get());

  if (observer_)
    observer_->SetClient(this);

  memory_usage_reporting_timer_.SetTaskRunner(
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia));

  if (frame_->IsAdSubframe())
    media_metrics_provider_->SetIsAdMedia();

#if defined(OS_ANDROID)
  renderer_factory_selector_->SetRemotePlayStateChangeCB(base::BindRepeating(
      &WebMediaPlayerImpl::OnRemotePlayStateChange, weak_this_));
#endif  // defined (OS_ANDROID)
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (set_cdm_result_) {
    DVLOG(2)
        << "Resolve pending SetCdmInternal() when media player is destroyed.";
    set_cdm_result_->Complete();
    set_cdm_result_.reset();
  }

  suppress_destruction_errors_ = true;

  delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);

  // Finalize any watch time metrics before destroying the pipeline.
  watch_time_reporter_.reset();

  // The underlying Pipeline must be stopped before it is destroyed.
  //
  // Note: This destruction happens synchronously on the media thread and
  // |demuxer_|, |data_source_|, |compositor_|, and |media_log_| must outlive
  // this process. They will be destructed by the DestructionHelper below
  // after trampolining through the media thread.
  pipeline_controller_->Stop();

  if (last_reported_memory_usage_)
    adjust_allocated_memory_cb_.Run(-last_reported_memory_usage_);

  // Destruct compositor resources in the proper order.
  client_->SetCcLayer(nullptr);

  client_->MediaRemotingStopped(
      blink::WebLocalizedString::kMediaRemotingStopNoText);

  if (!surface_layer_for_video_enabled_ && video_layer_)
    video_layer_->StopUsingProvider();

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (data_source_)
    data_source_->Stop();

  // Disconnect from the surface layer. We still preserve the |bridge_| until
  // after pipeline shutdown to ensure any pending frames are painted for tests.
  if (bridge_)
    bridge_->ClearObserver();

  // Disconnect from the MediaObserver implementation since it's lifetime is
  // tied to the RendererFactorySelector which can't be destroyed until after
  // the Pipeline stops.
  //
  // Note: We can't use a WeakPtr with the RendererFactory because its methods
  // are called on the media thread and this destruction takes place on the
  // renderer thread.
  if (observer_)
    observer_->SetClient(nullptr);

  // Handle destruction of things that need to be destructed after the pipeline
  // completes stopping on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DestructionHelper, std::move(main_task_runner_),
                     std::move(vfc_task_runner_), std::move(demuxer_),
                     std::move(data_source_), std::move(compositor_),
                     std::move(cdm_context_ref_),
                     std::move(pending_cdm_context_ref_), std::move(media_log_),
                     std::move(renderer_factory_selector_), std::move(bridge_),
                     !!chunk_demuxer_));
}

WebMediaPlayer::LoadTiming WebMediaPlayerImpl::Load(
    LoadType load_type,
    const blink::WebMediaPlayerSource& source,
    CorsMode cors_mode) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();
  DVLOG(1) << __func__ << "(" << load_type << ", " << GURL(url) << ", "
           << cors_mode << ")";

  bool is_deferred = false;

  if (defer_load_cb_) {
    is_deferred = defer_load_cb_.Run(base::BindOnce(
        &WebMediaPlayerImpl::DoLoad, weak_this_, load_type, url, cors_mode));
  } else {
    DoLoad(load_type, url, cors_mode);
  }

  return is_deferred ? LoadTiming::kDeferred : LoadTiming::kImmediate;
}

void WebMediaPlayerImpl::OnWebLayerUpdated() {}

void WebMediaPlayerImpl::RegisterContentsLayer(cc::Layer* layer) {
  DCHECK(bridge_);
  bridge_->SetContentsOpaque(opaque_);
  client_->SetCcLayer(layer);
}

void WebMediaPlayerImpl::UnregisterContentsLayer(cc::Layer* layer) {
  // |client_| will unregister its cc::Layer if given a nullptr.
  client_->SetCcLayer(nullptr);
}

void WebMediaPlayerImpl::OnSurfaceIdUpdated(viz::SurfaceId surface_id) {
  // TODO(726619): Handle the behavior when Picture-in-Picture mode is
  // disabled.
  // The viz::SurfaceId may be updated when the video begins playback or when
  // the size of the video changes.
  if (client_)
    client_->OnPictureInPictureStateChange();
}

bool WebMediaPlayerImpl::SupportsOverlayFullscreenVideo() {
#if defined(OS_ANDROID)
  return !using_media_player_renderer_ &&
         overlay_mode_ == OverlayMode::kUseContentVideoView;
#else
  return false;
#endif
}

void WebMediaPlayerImpl::EnableOverlay() {
  overlay_enabled_ = true;
  if (request_routing_token_cb_ &&
      overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    overlay_routing_token_is_pending_ = true;
    token_available_cb_.Reset(
        base::Bind(&WebMediaPlayerImpl::OnOverlayRoutingToken, weak_this_));
    request_routing_token_cb_.Run(token_available_cb_.callback());
  }

  // We have requested (and maybe already have) overlay information.  If the
  // restarted decoder requests overlay information, then we'll defer providing
  // it if it hasn't arrived yet.  Otherwise, this would be a race, since we
  // don't know if the request for overlay info or restart will complete first.
  if (decoder_requires_restart_for_overlay_)
    ScheduleRestart();
}

void WebMediaPlayerImpl::DisableOverlay() {
  overlay_enabled_ = false;
  if (overlay_mode_ == OverlayMode::kUseContentVideoView) {
    surface_created_cb_.Cancel();
  } else if (overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    token_available_cb_.Cancel();
    overlay_routing_token_is_pending_ = false;
    overlay_routing_token_ = OverlayInfo::RoutingToken();
  }

  if (decoder_requires_restart_for_overlay_)
    ScheduleRestart();
  else
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::EnteredFullscreen() {
  overlay_info_.is_fullscreen = true;

  // |always_enable_overlays_| implies that we're already in overlay mode, so
  // take no action here.  Otherwise, switch to an overlay if it's allowed and
  // if it will display properly.
  if (!always_enable_overlays_ && overlay_mode_ != OverlayMode::kNoOverlays &&
      DoesOverlaySupportMetadata()) {
    EnableOverlay();
  }

  // We send this only if we can send multiple calls.  Otherwise, either (a)
  // we already sent it and we don't have a callback anyway (we reset it when
  // it's called in restart mode), or (b) we'll send this later when the surface
  // actually arrives.  GVD assumes that the first overlay info will have the
  // routing information.  Note that we set |is_fullscreen_| earlier, so that
  // if EnableOverlay() can include fullscreen info in case it sends the overlay
  // info before returning.
  if (!decoder_requires_restart_for_overlay_)
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::ExitedFullscreen() {
  overlay_info_.is_fullscreen = false;

  // If we're in overlay mode, then exit it unless we're supposed to allow
  // overlays all the time.
  if (!always_enable_overlays_ && overlay_enabled_)
    DisableOverlay();

  // See EnteredFullscreen for why we do this.
  if (!decoder_requires_restart_for_overlay_)
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::BecameDominantVisibleContent(bool isDominant) {
  if (observer_)
    observer_->OnBecameDominantVisibleContent(isDominant);
}

void WebMediaPlayerImpl::SetIsEffectivelyFullscreen(
    blink::WebFullscreenVideoStatus fullscreen_video_status) {
  delegate_->SetIsEffectivelyFullscreen(delegate_id_, fullscreen_video_status);
}

void WebMediaPlayerImpl::OnHasNativeControlsChanged(bool has_native_controls) {
  if (!watch_time_reporter_)
    return;

  if (has_native_controls)
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();
}

void WebMediaPlayerImpl::OnDisplayTypeChanged(
    WebMediaPlayer::DisplayType display_type) {
  if (surface_layer_for_video_enabled_) {
    vfc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoFrameCompositor::SetForceSubmit,
            base::Unretained(compositor_.get()),
            display_type == WebMediaPlayer::DisplayType::kPictureInPicture));
  }

  if (!watch_time_reporter_)
    return;

  switch (display_type) {
    case WebMediaPlayer::DisplayType::kInline:
      watch_time_reporter_->OnDisplayTypeInline();
      break;
    case WebMediaPlayer::DisplayType::kFullscreen:
      watch_time_reporter_->OnDisplayTypeFullscreen();
      break;
    case WebMediaPlayer::DisplayType::kPictureInPicture:
      watch_time_reporter_->OnDisplayTypePictureInPicture();

      // Resumes playback if it was paused when hidden.
      if (paused_when_hidden_) {
        paused_when_hidden_ = false;
        OnPlay();
      }
      break;
  }
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CorsMode cors_mode) {
  TRACE_EVENT1("media", "WebMediaPlayerImpl::DoLoad", "id", media_log_->id());
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)
  // Only allow credentials if the crossorigin attribute is unspecified
  // (kCorsModeUnspecified) or "use-credentials" (kCorsModeUseCredentials).
  // This value is only used by the MediaPlayerRenderer.
  // See https://crbug.com/936566.
  //
  // The credentials mode also has repercussions in WouldTaintOrigin(), but we
  // access what we need from |mb_data_source_|->cors_mode() directly, instead
  // of storing it here.
  allow_media_player_renderer_credentials_ = cors_mode != kCorsModeAnonymous;
#endif  // defined(OS_ANDROID)

  // Note: |url| may be very large, take care when making copies.
  loaded_url_ = GURL(url);
  load_type_ = load_type;

  ReportMetrics(load_type, loaded_url_, *frame_, media_log_.get());

  // Report poster availability for SRC=.
  if (load_type == kLoadTypeURL) {
    if (preload_ == MultibufferDataSource::METADATA) {
      UMA_HISTOGRAM_BOOLEAN("Media.SRC.PreloadMetaDataHasPoster", has_poster_);
    } else if (preload_ == MultibufferDataSource::AUTO) {
      UMA_HISTOGRAM_BOOLEAN("Media.SRC.PreloadAutoHasPoster", has_poster_);
    }
  }

  // Set subresource URL for crash reporting; will be truncated to 256 bytes.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url, loaded_url_.spec());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.GetString().Utf8()));
  load_start_time_ = base::TimeTicks::Now();

  media_metrics_provider_->Initialize(load_type == kLoadTypeMediaSource,
                                      load_type == kLoadTypeURL
                                          ? GetMediaURLScheme(loaded_url_)
                                          : mojom::MediaURLScheme::kUnknown);

  // Media source pipelines can start immediately.
  if (load_type == kLoadTypeMediaSource) {
    StartPipeline();
  } else {
    // Short circuit the more complex loading path for data:// URLs. Sending
    // them through the network based loading path just wastes memory and causes
    // worse performance since reads become asynchronous.
    if (loaded_url_.SchemeIs(url::kDataScheme)) {
      std::string mime_type, charset, data;
      if (!net::DataURL::Parse(loaded_url_, &mime_type, &charset, &data) ||
          data.empty()) {
        DataSourceInitialized(false);
        return;
      }

      // Replace |loaded_url_| with an empty data:// URL since it may be large.
      loaded_url_ = GURL("data:,");

      // Mark all the data as buffered.
      buffered_data_source_host_->SetTotalBytes(data.size());
      buffered_data_source_host_->AddBufferedByteRange(0, data.size());

      DCHECK(!mb_data_source_);
      data_source_.reset(new MemoryDataSource(std::move(data)));
      DataSourceInitialized(true);
      return;
    }

    auto url_data =
        url_index_->GetByUrl(url, static_cast<UrlData::CorsMode>(cors_mode));
    // Notify |this| of bytes received by the network.
    url_data->AddBytesReceivedCallback(BindToCurrentLoop(
        base::BindRepeating(&WebMediaPlayerImpl::OnBytesReceived, weak_this_)));
    mb_data_source_ = new MultibufferDataSource(
        main_task_runner_, std::move(url_data), media_log_.get(),
        buffered_data_source_host_.get(),
        base::BindRepeating(&WebMediaPlayerImpl::NotifyDownloading,
                            weak_this_));
    data_source_.reset(mb_data_source_);
    mb_data_source_->SetPreload(preload_);
    mb_data_source_->SetIsClientAudioElement(client_->IsAudioElement());
    mb_data_source_->Initialize(
        base::Bind(&WebMediaPlayerImpl::DataSourceInitialized, weak_this_));
  }
}

void WebMediaPlayerImpl::Play() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // User initiated play unlocks background video playback.
  if (blink::WebUserGestureIndicator::IsProcessingUserGesture(frame_))
    video_locked_when_paused_when_hidden_ = false;

  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  paused_ = false;
  pipeline_controller_->SetPlaybackRate(playback_rate_);
  background_pause_timer_.Stop();

  if (observer_)
    observer_->OnPlaying();

  watch_time_reporter_->SetAutoplayInitiated(client_->WasAutoplayInitiated());

  // If we're seeking we'll trigger the watch time reporter upon seek completed;
  // we don't want to start it here since the seek time is unstable. E.g., when
  // playing content with a positive start time we would have a zero seek time.
  if (!Seeking()) {
    DCHECK(watch_time_reporter_);
    watch_time_reporter_->OnPlaying();
  }

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnPlaying();

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PLAY));

  MaybeUpdateBufferSizesForPlayback();
  UpdatePlayState();
}

void WebMediaPlayerImpl::Pause() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // We update the paused state even when casting, since we expect pause() to be
  // called when casting begins, and when we exit casting we should end up in a
  // paused state.
  paused_ = true;

  // No longer paused because it was hidden.
  paused_when_hidden_ = false;

  // User initiated pause locks background videos.
  if (blink::WebUserGestureIndicator::IsProcessingUserGesture(frame_))
    video_locked_when_paused_when_hidden_ = true;

  pipeline_controller_->SetPlaybackRate(0.0);

  // For states <= kReadyStateHaveMetadata, we may not have a renderer yet.
  if (highest_ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    paused_time_ = pipeline_controller_->GetMediaTime();

  if (observer_)
    observer_->OnPaused();

  DCHECK(watch_time_reporter_);
  watch_time_reporter_->OnPaused();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnPaused();

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PAUSE));

  UpdatePlayState();
}

void WebMediaPlayerImpl::Seek(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << "s)";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(
      media_log_->CreateTimeEvent(MediaLogEvent::SEEK, "seek_target", seconds));
  DoSeek(base::TimeDelta::FromSecondsD(seconds), true);
}

void WebMediaPlayerImpl::DoSeek(base::TimeDelta time, bool time_updated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT2("media", "WebMediaPlayerImpl::DoSeek", "target",
               time.InSecondsF(), "id", media_log_->id());

  ReadyState old_state = ready_state_;
  if (ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);

  // When paused or ended, we know exactly what the current time is and can
  // elide seeks to it. However, there are two cases that are not elided:
  //   1) When the pipeline state is not stable.
  //      In this case we just let |pipeline_controller_| decide what to do, as
  //      it has complete information.
  //   2) For MSE.
  //      Because the buffers may have changed between seeks, MSE seeks are
  //      never elided.
  if (paused_ && pipeline_controller_->IsStable() &&
      (paused_time_ == time ||
       (ended_ && time == base::TimeDelta::FromSecondsD(Duration()))) &&
      !chunk_demuxer_) {
    // If the ready state was high enough before, we can indicate that the seek
    // completed just by restoring it. Otherwise we will just wait for the real
    // ready state change to eventually happen.
    if (old_state == kReadyStateHaveEnoughData) {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WebMediaPlayerImpl::OnBufferingStateChange,
                                    weak_this_, BUFFERING_HAVE_ENOUGH));
    }
    return;
  }

  // Call this before setting |seeking_| so that the current media time can be
  // recorded by the reporter.
  if (watch_time_reporter_)
    watch_time_reporter_->OnSeeking();

  // TODO(sandersd): Move |seeking_| to PipelineController.
  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  ended_ = false;
  seeking_ = true;
  seek_time_ = time;
  if (paused_)
    paused_time_ = time;
  pipeline_controller_->Seek(time, time_updated);

  // This needs to be called after Seek() so that if a resume is triggered, it
  // is to the correct time.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetRate(double rate) {
  DVLOG(1) << __func__ << "(" << rate << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (rate != playback_rate_) {
    LIMITED_MEDIA_LOG(INFO, media_log_.get(), num_playback_rate_logs_,
                      kMaxNumPlaybackRateLogs)
        << "Effective playback rate changed from " << playback_rate_ << " to "
        << rate;
  }

  playback_rate_ = rate;
  if (!paused_)
    pipeline_controller_->SetPlaybackRate(rate);

  MaybeUpdateBufferSizesForPlayback();
}

void WebMediaPlayerImpl::SetVolume(double volume) {
  DVLOG(1) << __func__ << "(" << volume << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  volume_ = volume;
  pipeline_controller_->SetVolume(volume_ * volume_multiplier_);
  if (watch_time_reporter_)
    watch_time_reporter_->OnVolumeChange(volume);
  delegate_->DidPlayerMutedStatusChange(delegate_id_, volume == 0.0);

  // The play state is updated because the player might have left the autoplay
  // muted state.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnRequestPictureInPicture() {
  if (!surface_layer_for_video_enabled_)
    ActivateSurfaceLayerForVideo();

  DCHECK(bridge_);
  DCHECK(bridge_->GetSurfaceId().is_valid());
}

void WebMediaPlayerImpl::SetSinkId(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  OutputDeviceStatusCB callback =
      ConvertToOutputDeviceStatusCB(std::move(completion_callback));
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SetSinkIdOnMediaThread, audio_source_provider_,
                                sink_id.Utf8(), std::move(callback)));
}

STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadNone, MultibufferDataSource::NONE);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadMetaData,
                   MultibufferDataSource::METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadAuto, MultibufferDataSource::AUTO);

void WebMediaPlayerImpl::SetPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __func__ << "(" << preload << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  preload_ = static_cast<MultibufferDataSource::Preload>(preload);
  if (mb_data_source_)
    mb_data_source_->SetPreload(preload_);
}

bool WebMediaPlayerImpl::HasVideo() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_video;
}

bool WebMediaPlayerImpl::HasAudio() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_metadata_.has_audio;
}

void WebMediaPlayerImpl::EnabledAudioTracksChanged(
    const blink::WebVector<blink::WebMediaPlayer::TrackId>& enabledTrackIds) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::ostringstream logstr;
  std::vector<MediaTrack::Id> enabledMediaTrackIds;
  for (const auto& blinkTrackId : enabledTrackIds) {
    const auto track_id = MediaTrack::Id(blinkTrackId.Utf8().data());
    logstr << track_id << " ";
    enabledMediaTrackIds.push_back(track_id);
  }
  MEDIA_LOG(INFO, media_log_.get())
      << "Enabled audio tracks: [" << logstr.str() << "]";
  pipeline_controller_->OnEnabledAudioTracksChanged(enabledMediaTrackIds);
}

void WebMediaPlayerImpl::SelectedVideoTrackChanged(
    blink::WebMediaPlayer::TrackId* selectedTrackId) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::Optional<MediaTrack::Id> selected_video_track_id;
  if (selectedTrackId && !video_track_disabled_)
    selected_video_track_id = MediaTrack::Id(selectedTrackId->Utf8().data());
  MEDIA_LOG(INFO, media_log_.get())
      << "Selected video track: ["
      << selected_video_track_id.value_or(MediaTrack::Id()) << "]";
  pipeline_controller_->OnSelectedVideoTrackChanged(selected_video_track_id);
}

blink::WebSize WebMediaPlayerImpl::NaturalSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return blink::WebSize(pipeline_metadata_.natural_size);
}

blink::WebSize WebMediaPlayerImpl::VisibleRect() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame)
    return blink::WebSize();

  const gfx::Rect& visible_rect = video_frame->visible_rect();
  return blink::WebSize(visible_rect.width(), visible_rect.height());
}

bool WebMediaPlayerImpl::Paused() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return pipeline_controller_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::PausedWhenHidden() const {
  return paused_when_hidden_;
}

bool WebMediaPlayerImpl::Seeking() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return false;

  return seeking_;
}

double WebMediaPlayerImpl::Duration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  // Use duration from ChunkDemuxer when present. MSE allows users to specify
  // duration as a double. This propagates to the rest of the pipeline as a
  // TimeDelta with potentially reduced precision (limited to Microseconds).
  // ChunkDemuxer returns the full-precision user-specified double. This ensures
  // users can "get" the exact duration they "set".
  if (chunk_demuxer_)
    return chunk_demuxer_->GetDuration();

  base::TimeDelta pipeline_duration = GetPipelineMediaDuration();
  return pipeline_duration == kInfiniteDuration
             ? std::numeric_limits<double>::infinity()
             : pipeline_duration.InSecondsF();
}

double WebMediaPlayerImpl::timelineOffset() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (pipeline_metadata_.timeline_offset.is_null())
    return std::numeric_limits<double>::quiet_NaN();

  return pipeline_metadata_.timeline_offset.ToJsTime();
}

base::TimeDelta WebMediaPlayerImpl::GetCurrentTimeInternal() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::TimeDelta current_time;
  if (Seeking())
    current_time = seek_time_;
  else if (paused_)
    current_time = paused_time_;
  else
    current_time = pipeline_controller_->GetMediaTime();

  // It's possible for |current_time| to be kInfiniteDuration here if the page
  // seeks to kInfiniteDuration (2**64 - 1) when Duration() is infinite.
  DCHECK_GE(current_time, base::TimeDelta());
  return current_time;
}

double WebMediaPlayerImpl::CurrentTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  // Note: Duration() may be infinity.
  return (ended_ && !std::isinf(Duration()))
             ? Duration()
             : GetCurrentTimeInternal().InSecondsF();
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::GetNetworkState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::GetReadyState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ready_state_;
}

blink::WebMediaPlayer::SurfaceLayerMode
WebMediaPlayerImpl::GetVideoSurfaceLayerMode() const {
  return surface_layer_mode_;
}

blink::WebString WebMediaPlayerImpl::GetErrorMessage() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerImpl::Buffered() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_controller_->GetBufferedTimeRanges();

  const base::TimeDelta duration = GetPipelineMediaDuration();
  if (duration != kInfiniteDuration) {
    buffered_data_source_host_->AddBufferedTimeRanges(&buffered_time_ranges,
                                                      duration);
  }
  return ConvertToWebTimeRanges(buffered_time_ranges);
}

blink::WebTimeRanges WebMediaPlayerImpl::Seekable() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  const double seekable_end = Duration();

  // Allow a special exception for seeks to zero for streaming sources with a
  // finite duration; this allows looping to work.
  const bool is_finite_stream = mb_data_source_ &&
                                mb_data_source_->IsStreaming() &&
                                std::isfinite(seekable_end);

  // Do not change the seekable range when using the MediaPlayerRenderer. It
  // will take care of dropping invalid seeks.
  const bool force_seeks_to_zero =
      !using_media_player_renderer_ && is_finite_stream;

  // TODO(dalecurtis): Technically this allows seeking on media which return an
  // infinite duration so long as DataSource::IsStreaming() is false. While not
  // expected, disabling this breaks semi-live players, http://crbug.com/427412.
  const blink::WebTimeRange seekable_range(
      0.0, force_seeks_to_zero ? 0.0 : seekable_end);
  return blink::WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerImpl::IsPrerollAttemptNeeded() {
  // TODO(sandersd): Replace with |highest_ready_state_since_seek_| if we need
  // to ensure that preroll always gets a chance to complete.
  // See http://crbug.com/671525.
  //
  // Note: Even though we get play/pause signals at kReadyStateHaveMetadata, we
  // must attempt to preroll until kReadyStateHaveFutureData so that the
  // canplaythrough event will be fired to the page (which may be waiting).
  //
  // TODO(dalecurtis): We should try signaling kReadyStateHaveFutureData upon
  // automatic-suspend of a non-playing element to avoid wasting resources.
  if (highest_ready_state_ >= ReadyState::kReadyStateHaveFutureData)
    return false;

  // To suspend before we reach kReadyStateHaveCurrentData is only ok
  // if we know we're going to get woken up when we get more data, which
  // will only happen if the network is in the "Loading" state.
  // This happens when the network is fast, but multiple videos are loading
  // and multiplexing gets held up waiting for available threads.
  if (highest_ready_state_ <= ReadyState::kReadyStateHaveMetadata &&
      network_state_ != WebMediaPlayer::kNetworkStateLoading) {
    return true;
  }

  if (preroll_attempt_pending_)
    return true;

  // Freshly initialized; there has never been any loading progress. (Otherwise
  // |preroll_attempt_pending_| would be true when the start time is null.)
  if (preroll_attempt_start_time_.is_null())
    return false;

  base::TimeDelta preroll_attempt_duration =
      tick_clock_->NowTicks() - preroll_attempt_start_time_;
  return preroll_attempt_duration < kPrerollAttemptTimeout;
}

bool WebMediaPlayerImpl::DidLoadingProgress() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Note: Separate variables used to ensure both methods are called every time.
  const bool pipeline_progress = pipeline_controller_->DidLoadingProgress();
  const bool data_progress = buffered_data_source_host_->DidLoadingProgress();
  return pipeline_progress || data_progress;
}

void WebMediaPlayerImpl::Paint(cc::PaintCanvas* canvas,
                               const blink::WebRect& rect,
                               cc::PaintFlags& flags,
                               int already_uploaded_id,
                               VideoFrameUploadMetadata* out_metadata) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");

  // We can't copy from protected frames.
  if (cdm_context_ref_)
    return;

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();

  gfx::Rect gfx_rect(rect);
  if (video_frame.get() && video_frame->HasTextures()) {
    if (!context_provider_)
      return;  // Unable to get/create a shared main thread context.
    if (!context_provider_->GrContext())
      return;  // The context has been lost since and can't setup a GrContext.
  }
  if (out_metadata && video_frame) {
    // WebGL last-uploaded-frame-metadata API enabled. https://crbug.com/639174
    ComputeFrameUploadMetadata(video_frame.get(), already_uploaded_id,
                               out_metadata);
    if (out_metadata->skipped) {
      // Skip uploading this frame.
      return;
    }
  }
  video_renderer_.Paint(
      video_frame, canvas, gfx::RectF(gfx_rect), flags,
      pipeline_metadata_.video_decoder_config.video_transformation(),
      context_provider_.get());
}

bool WebMediaPlayerImpl::WouldTaintOrigin() const {
  if (demuxer_found_hls_) {
    // HLS manifests might pull segments from a different origin. We can't know
    // for sure, so we conservatively say no here.
    return true;
  }

  if (!mb_data_source_)
    return false;

  // When the resource is redirected to another origin we think it as
  // tainted. This is actually not specified, and is under discussion.
  // See https://github.com/whatwg/fetch/issues/737.
  if (!mb_data_source_->HasSingleOrigin() &&
      mb_data_source_->cors_mode() == UrlData::CORS_UNSPECIFIED) {
    return true;
  }

  return mb_data_source_->IsCorsCrossOrigin();
}

double WebMediaPlayerImpl::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::DecodedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_frames_decoded;
}

unsigned WebMediaPlayerImpl::DroppedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_frames_dropped;
}

uint64_t WebMediaPlayerImpl::AudioDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().audio_bytes_decoded;
}

uint64_t WebMediaPlayerImpl::VideoDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return GetPipelineStatistics().video_bytes_decoded;
}

bool WebMediaPlayerImpl::HasAvailableVideoFrame() const {
  return has_first_frame_;
}

bool WebMediaPlayerImpl::CopyVideoTextureToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    unsigned int target,
    unsigned int texture,
    unsigned internal_format,
    unsigned format,
    unsigned type,
    int level,
    bool premultiply_alpha,
    bool flip_y,
    int already_uploaded_id,
    VideoFrameUploadMetadata* out_metadata) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:copyVideoTextureToPlatformTexture");

  // We can't copy from protected frames.
  if (cdm_context_ref_)
    return false;

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame.get() || !video_frame->HasTextures()) {
    return false;
  }
  if (out_metadata) {
    // WebGL last-uploaded-frame-metadata API is enabled.
    // https://crbug.com/639174
    ComputeFrameUploadMetadata(video_frame.get(), already_uploaded_id,
                               out_metadata);
    if (out_metadata->skipped) {
      // Skip uploading this frame.
      return true;
    }
  }

  return video_renderer_.CopyVideoFrameTexturesToGLTexture(
      context_provider_.get(), gl, video_frame.get(), target, texture,
      internal_format, format, type, level, premultiply_alpha, flip_y);
}

bool WebMediaPlayerImpl::PrepareVideoFrameForWebGL(
    gpu::gles2::GLES2Interface* gl,
    unsigned target,
    unsigned texture,
    int already_uploaded_id,
    WebMediaPlayer::VideoFrameUploadMetadata* out_metadata) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::PrepareVideoFrameForWebGL");

  // TODO(crbug.com/776222): How to deal with protected frames.
  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame.get() || !video_frame->HasTextures()) {
    return false;
  }
  if (out_metadata) {
    // WebGL last-uploaded-frame-metadata API is enabled.
    ComputeFrameUploadMetadata(video_frame.get(), already_uploaded_id,
                               out_metadata);
    if (out_metadata->skipped) {
      // Skip uploading this frame.
      return true;
    }
  }

  return video_renderer_.PrepareVideoFrameForWebGL(
      context_provider_.get(), gl, video_frame.get(), target, texture);
}

// static
void WebMediaPlayerImpl::ComputeFrameUploadMetadata(
    VideoFrame* frame,
    int already_uploaded_id,
    VideoFrameUploadMetadata* out_metadata) {
  DCHECK(out_metadata);
  DCHECK(frame);
  out_metadata->frame_id = frame->unique_id();
  out_metadata->visible_rect = frame->visible_rect();
  out_metadata->timestamp = frame->timestamp();
  bool skip_possible = already_uploaded_id != -1;
  bool same_frame_id = frame->unique_id() == already_uploaded_id;
  out_metadata->skipped = skip_possible && same_frame_id;
}

void WebMediaPlayerImpl::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {
  DVLOG(1) << __func__ << ": cdm = " << cdm;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Once the CDM is set it can't be cleared as there may be frames being
  // decrypted on other threads. So fail this request.
  // http://crbug.com/462365#c7.
  if (!cdm) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionInvalidStateError, 0,
        "The existing ContentDecryptionModule object cannot be removed at this "
        "time.");
    return;
  }

  // Create a local copy of |result| to avoid problems with the callback
  // getting passed to the media thread and causing |result| to be destructed
  // on the wrong thread in some failure conditions. Blink should prevent
  // multiple simultaneous calls.
  DCHECK(!set_cdm_result_);
  set_cdm_result_.reset(new blink::WebContentDecryptionModuleResult(result));

  SetCdmInternal(cdm);
}

void WebMediaPlayerImpl::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(init_data_type != EmeInitDataType::UNKNOWN);

  RecordEncryptedEvent(true);

  // Recreate the watch time reporter if necessary.
  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;
  if (!was_encrypted) {
    media_metrics_provider_->SetIsEME();
    if (watch_time_reporter_)
      CreateWatchTimeReporter();

    // |was_encrypted| = false means we didn't have a CDM prior to observing
    // encrypted media init data. Reset the reporter until the CDM arrives. See
    // SetCdmInternal().
    DCHECK(!cdm_config_);
    video_decode_stats_reporter_.reset();
  }

  encrypted_client_->Encrypted(
      ConvertToWebInitDataType(init_data_type), init_data.data(),
      base::saturated_cast<unsigned int>(init_data.size()));
}

void WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated(
    std::unique_ptr<MediaTracks> tracks) {
  // For MSE/chunk_demuxer case the media track updates are handled by
  // WebSourceBufferImpl.
  DCHECK(demuxer_.get());
  DCHECK(!chunk_demuxer_);

  // Report the media track information to blink. Only the first audio track and
  // the first video track are enabled by default to match blink logic.
  bool is_first_audio_track = true;
  bool is_first_video_track = true;
  for (const auto& track : tracks->tracks()) {
    if (track->type() == MediaTrack::Audio) {
      client_->AddAudioTrack(
          blink::WebString::FromUTF8(track->id().value()),
          blink::WebMediaPlayerClient::kAudioTrackKindMain,
          blink::WebString::FromUTF8(track->label().value()),
          blink::WebString::FromUTF8(track->language().value()),
          is_first_audio_track);
      is_first_audio_track = false;
    } else if (track->type() == MediaTrack::Video) {
      client_->AddVideoTrack(
          blink::WebString::FromUTF8(track->id().value()),
          blink::WebMediaPlayerClient::kVideoTrackKindMain,
          blink::WebString::FromUTF8(track->label().value()),
          blink::WebString::FromUTF8(track->language().value()),
          is_first_video_track);
      is_first_video_track = false;
    } else {
      // Text tracks are not supported through this code path yet.
      NOTREACHED();
    }
  }
}

void WebMediaPlayerImpl::SetCdmInternal(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(cdm);

  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;

  // Recreate the watch time reporter if necessary.
  if (!was_encrypted) {
    media_metrics_provider_->SetIsEME();
    if (watch_time_reporter_)
      CreateWatchTimeReporter();
  }

  WebContentDecryptionModuleImpl* web_cdm =
      ToWebContentDecryptionModuleImpl(cdm);
  auto cdm_context_ref = web_cdm->GetCdmContextRef();
  if (!cdm_context_ref) {
    NOTREACHED();
    OnCdmAttached(false);
    return;
  }

  // Arrival of |cdm_config_| and |key_system_| unblocks recording of encrypted
  // stats. Attempt to create the stats reporter. Note, we do NOT guard this
  // within !was_encypted above because often the CDM arrives after the call to
  // OnEncryptedMediaInitData().
  cdm_config_ = web_cdm->GetCdmConfig();
  key_system_ = web_cdm->GetKeySystem();
  DCHECK(!key_system_.empty());
  CreateVideoDecodeStatsReporter();

  CdmContext* cdm_context = cdm_context_ref->GetCdmContext();
  DCHECK(cdm_context);

  // Keep the reference to the CDM, as it shouldn't be destroyed until
  // after the pipeline is done with the |cdm_context|.
  pending_cdm_context_ref_ = std::move(cdm_context_ref);
  pipeline_controller_->SetCdm(
      cdm_context, base::Bind(&WebMediaPlayerImpl::OnCdmAttached, weak_this_));
}

void WebMediaPlayerImpl::OnCdmAttached(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_cdm_context_ref_);

  // If the CDM is set from the constructor there is no promise
  // (|set_cdm_result_|) to fulfill.
  if (success) {
    media_log_->SetBooleanProperty("has_cdm", true);

    // This will release the previously attached CDM (if any).
    cdm_context_ref_ = std::move(pending_cdm_context_ref_);
    if (set_cdm_result_) {
      set_cdm_result_->Complete();
      set_cdm_result_.reset();
    }

    return;
  }

  pending_cdm_context_ref_.reset();
  if (set_cdm_result_) {
    set_cdm_result_->CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Unable to set ContentDecryptionModule object");
    set_cdm_result_.reset();
  }
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_updated) {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnPipelineSeeked", "target",
               seek_time_.InSecondsF(), "id", media_log_->id());
  seeking_ = false;
  seek_time_ = base::TimeDelta();

  if (paused_) {
    paused_time_ = pipeline_controller_->GetMediaTime();
  } else {
    DCHECK(watch_time_reporter_);
    watch_time_reporter_->OnPlaying();
  }
  if (time_updated)
    should_notify_time_changed_ = true;

  // Reset underflow duration upon seek; this prevents looping videos and user
  // actions from artificially inflating the duration.
  underflow_timer_.reset();

  // Background video optimizations are delayed when shown/hidden if pipeline
  // is seeking.
  UpdateBackgroundVideoOptimizationState();

  // If we successfully completed a suspended startup, we need to make a call to
  // UpdatePlayState() in case any events which should trigger a resume have
  // occurred during startup.
  if (attempting_suspended_start_ &&
      pipeline_controller_->IsPipelineSuspended()) {
    skip_metrics_due_to_startup_suspend_ = true;

    // If we successfully completed a suspended startup, signal that we have
    // reached BUFFERING_HAVE_ENOUGH so that canplay and canplaythrough fire
    // correctly. We must unfortunately always do this because it's valid for
    // elements to play while not visible nor even in the DOM.
    //
    // Note: This call is dual purpose, it is also responsible for triggering an
    // UpdatePlayState() call which may need to resume the pipeline once Blink
    // has been told about the ReadyState change.
    OnBufferingStateChangeInternal(BUFFERING_HAVE_ENOUGH, true);

    // If |skip_metrics_due_to_startup_suspend_| is unset by a resume started by
    // the OnBufferingStateChangeInternal() call, record a histogram of it here.
    //
    // If the value is unset, that means we should not have suspended and we've
    // likely incurred some cost to TimeToFirstFrame and TimeToPlayReady which
    // will be reflected in those statistics.
    base::UmaHistogramBoolean(
        std::string("Media.PreloadMetadataSuspendWasIdeal.") +
            ((HasVideo() && HasAudio()) ? "AudioVideo"
                                        : (HasVideo() ? "Video" : "Audio")),
        skip_metrics_due_to_startup_suspend_);
  }

  attempting_suspended_start_ = false;
}

void WebMediaPlayerImpl::OnPipelineSuspended() {
  // Add a log event so the player shows up as "SUSPENDED" in media-internals.
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::SUSPENDED));

  if (attempting_suspended_start_) {
    DCHECK(pipeline_controller_->IsSuspended());
    did_lazy_load_ = !has_poster_ && HasVideo();
  }

  // Tell the data source we have enough data so that it may release the
  // connection (unless blink is waiting on us to signal play()).
  if (mb_data_source_ && !client_->CouldPlayIfEnoughData()) {
    // |attempting_suspended_start_| will be cleared by OnPipelineSeeked() which
    // will occur after this method during a suspended startup.
    if (attempting_suspended_start_ && did_lazy_load_) {
      DCHECK(!has_first_frame_);
      DCHECK(have_enough_after_lazy_load_cb_.IsCancelled());

      // For lazy load, we won't know if the element is non-visible until a
      // layout completes, so to avoid unnecessarily tearing down the network
      // connection, briefly (250ms chosen arbitrarily) delay signaling "have
      // enough" to the MultiBufferDataSource.
      //
      // base::Unretained() is safe here since the base::CancelableOnceClosure
      // will cancel upon destruction of this class and |mb_data_source_| is
      // gauranteeed to outlive us.
      have_enough_after_lazy_load_cb_.Reset(
          base::BindOnce(&MultibufferDataSource::OnBufferingHaveEnough,
                         base::Unretained(mb_data_source_), true));
      main_task_runner_->PostDelayedTask(
          FROM_HERE, have_enough_after_lazy_load_cb_.callback(),
          base::TimeDelta::FromMilliseconds(250));
    } else {
      have_enough_after_lazy_load_cb_.Cancel();
      mb_data_source_->OnBufferingHaveEnough(true);
    }
  }

  ReportMemoryUsage();

  if (pending_suspend_resume_cycle_) {
    pending_suspend_resume_cycle_ = false;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::OnBeforePipelineResume() {
  // Since we're resuming, cancel closing of the network connection.
  have_enough_after_lazy_load_cb_.Cancel();

  // We went through suspended startup, so the player is only just now spooling
  // up for playback. As such adjust |load_start_time_| so it reports the same
  // metric as what would be reported if we had not suspended at startup.
  if (skip_metrics_due_to_startup_suspend_) {
    // In the event that the call to SetReadyState() initiated after pipeline
    // startup immediately tries to start playback, we should not update
    // |load_start_time_| to avoid losing visibility into the impact of a
    // suspended startup on the time until first frame / play ready for cases
    // where suspended startup was applied incorrectly.
    if (!attempting_suspended_start_)
      load_start_time_ = base::TimeTicks::Now() - time_to_metadata_;
    skip_metrics_due_to_startup_suspend_ = false;
  }

  // Enable video track if we disabled it in the background - this way the new
  // renderer will attach its callbacks to the video stream properly.
  // TODO(avayvod): Remove this when disabling and enabling video tracks in
  // non-playing state works correctly. See https://crbug.com/678374.
  EnableVideoTrackIfNeeded();
  is_pipeline_resuming_ = true;
}

void WebMediaPlayerImpl::OnPipelineResumed() {
  is_pipeline_resuming_ = false;

  UpdateBackgroundVideoOptimizationState();
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_->MediaSourceOpened(new WebMediaSourceImpl(chunk_demuxer_));
}

void WebMediaPlayerImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DVLOG(2) << __func__ << " memory_pressure_level=" << memory_pressure_level;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC));
  DCHECK(chunk_demuxer_);

  // The new value of |memory_pressure_level| will take effect on the next
  // garbage collection. Typically this means the next SourceBuffer append()
  // operation, since per MSE spec, the garbage collection must only occur
  // during SourceBuffer append(). But if memory pressure is critical it might
  // be better to perform GC immediately rather than wait for the next append
  // and potentially get killed due to out-of-memory.
  // So if this experiment is enabled and pressure level is critical, we'll pass
  // down force_instant_gc==true, which will force immediate GC on
  // SourceBufferStreams.
  bool force_instant_gc =
      (enable_instant_source_buffer_gc_ &&
       memory_pressure_level ==
           base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // base::Unretained is safe, since |chunk_demuxer_| is actually owned by
  // |this| via this->demuxer_. Note the destruction of |chunk_demuxer_| is done
  // from ~WMPI by first hopping to |media_task_runner_| to prevent race with
  // this task.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ChunkDemuxer::OnMemoryPressure,
                                base::Unretained(chunk_demuxer_),
                                base::TimeDelta::FromSecondsD(CurrentTime()),
                                memory_pressure_level, force_instant_gc));
}

void WebMediaPlayerImpl::OnError(PipelineStatus status) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(status, PIPELINE_OK);

  if (suppress_destruction_errors_)
    return;

#if defined(OS_ANDROID)
  // |mb_data_source_| may be nullptr if someone passes in a m3u8 as a data://
  // URL, since MediaPlayer doesn't support data:// URLs, fail playback now.
  const bool found_hls = base::FeatureList::IsEnabled(kHlsPlayer) &&
                         status == PipelineStatus::DEMUXER_ERROR_DETECTED_HLS;
  if (found_hls && mb_data_source_) {
    demuxer_found_hls_ = true;

    UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.IsCorsCrossOrigin",
                          mb_data_source_->IsCorsCrossOrigin());
    if (mb_data_source_->IsCorsCrossOrigin()) {
      UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.HasAccessControl",
                            mb_data_source_->HasAccessControl());
    }

    // Note: Does not consider the full redirect chain, which could contain
    // undetected mixed content.
    bool frame_url_is_cryptographic = url::Origin(frame_->GetSecurityOrigin())
                                          .GetURL()
                                          .SchemeIsCryptographic();
    bool manifest_url_is_cryptographic =
        loaded_url_.SchemeIsCryptographic() &&
        mb_data_source_->GetUrlAfterRedirects().SchemeIsCryptographic();
    UMA_HISTOGRAM_BOOLEAN(
        "Media.WebMediaPlayerImpl.HLS.IsMixedContent",
        frame_url_is_cryptographic && !manifest_url_is_cryptographic);

    renderer_factory_selector_->SetUseMediaPlayer(true);

    loaded_url_ = mb_data_source_->GetUrlAfterRedirects();
    DCHECK(data_source_);
    data_source_->Stop();
    mb_data_source_ = nullptr;

    pipeline_controller_->Stop();
    SetMemoryReportingState(false);

    // Trampoline through the media task runner to destruct the demuxer and
    // data source now that we're switching to HLS playback.
    media_task_runner_->PostTask(
        FROM_HERE,
        BindToCurrentLoop(base::BindOnce(
            [](std::unique_ptr<Demuxer> demuxer,
               std::unique_ptr<DataSource> data_source,
               base::OnceClosure start_pipeline_cb) {
              // Release resources before starting HLS.
              demuxer.reset();
              data_source.reset();

              std::move(start_pipeline_cb).Run();
            },
            std::move(demuxer_), std::move(data_source_),
            base::BindOnce(&WebMediaPlayerImpl::StartPipeline, weak_this_))));

    return;
  }

  // We found hls in a data:// URL, fail immediately.
  if (found_hls)
    status = PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED;
#endif

  MaybeSetContainerName();
  ReportPipelineError(load_type_, status, media_log_.get());
  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(status));
  media_metrics_provider_->OnError(status);
  if (watch_time_reporter_)
    watch_time_reporter_->OnError(status);

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
  } else {
    SetNetworkState(PipelineErrorToNetworkState(status));
  }

  // PipelineController::Stop() is idempotent.
  pipeline_controller_->Stop();

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnEnded() {
  TRACE_EVENT2("media", "WebMediaPlayerImpl::OnEnded", "duration", Duration(),
               "id", media_log_->id());
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore state changes until we've completed all outstanding operations.
  if (!pipeline_controller_->IsStable())
    return;

  ended_ = true;
  client_->TimeChanged();

  // We don't actually want this to run until |client_| calls seek() or pause(),
  // but that should have already happened in timeChanged() and so this is
  // expected to be a no-op.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnMetadata(const PipelineMetadata& metadata) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Cache the |time_to_metadata_| to use for adjusting the TimeToFirstFrame and
  // TimeToPlayReady metrics later if we end up doing a suspended startup.
  time_to_metadata_ = base::TimeTicks::Now() - load_start_time_;
  media_metrics_provider_->SetTimeToMetadata(time_to_metadata_);
  RecordTimingUMA("Media.TimeToMetadata", time_to_metadata_);

  MaybeSetContainerName();

  pipeline_metadata_ = metadata;

  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoRotation",
      metadata.video_decoder_config.video_transformation().rotation,
      VIDEO_ROTATION_MAX + 1);

  if (HasAudio()) {
    RecordEncryptionScheme("Audio",
                           metadata.audio_decoder_config.encryption_scheme());
  }

  if (HasVideo()) {
    RecordEncryptionScheme("Video",
                           metadata.video_decoder_config.encryption_scheme());

    if (overlay_enabled_) {
      // SurfaceView doesn't support rotated video, so transition back if
      // the video is now rotated.  If |always_enable_overlays_|, we keep the
      // overlay anyway so that the state machine keeps working.
      // TODO(liberato): verify if compositor feedback catches this.  If so,
      // then we don't need this check.
      if (!always_enable_overlays_ && !DoesOverlaySupportMetadata())
        DisableOverlay();
    }

    if (surface_layer_mode_ ==
            blink::WebMediaPlayer::SurfaceLayerMode::kAlways ||
        (surface_layer_mode_ ==
             blink::WebMediaPlayer::SurfaceLayerMode::kOnDemand &&
         client_->DisplayType() ==
             WebMediaPlayer::DisplayType::kPictureInPicture)) {
      ActivateSurfaceLayerForVideo();
    } else {
      DCHECK(!video_layer_);
      // TODO(tmathmeyer) does this need support for reflections as well?
      video_layer_ = cc::VideoLayer::Create(
          compositor_.get(),
          pipeline_metadata_.video_decoder_config.video_transformation()
              .rotation);
      video_layer_->SetContentsOpaque(opaque_);
      client_->SetCcLayer(video_layer_.get());
    }
  }

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  // TODO(dalecurtis): Don't create these until kReadyStateHaveFutureData; when
  // we create them early we just increase the chances of needing to throw them
  // away unnecessarily.
  CreateWatchTimeReporter();
  CreateVideoDecodeStatsReporter();

  // SetReadyState() may trigger all sorts of calls into this class (e.g.,
  // Play(), Pause(), etc) so do it last to avoid unexpected states during the
  // calls. An exception to this is UpdatePlayState(), which is safe to call and
  // needs to use the new ReadyState in its calculations.
  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  UpdatePlayState();
}

void WebMediaPlayerImpl::ActivateSurfaceLayerForVideo() {
  // Note that we might or might not already be in VideoLayer mode.
  DCHECK(!bridge_);

  surface_layer_for_video_enabled_ = true;

  // If we're in VideoLayer mode, then get rid of the layer.
  if (video_layer_) {
    client_->SetCcLayer(nullptr);
    video_layer_ = nullptr;
  }

  bridge_ = std::move(create_bridge_callback_)
                .Run(this, compositor_->GetUpdateSubmissionStateCallback());
  bridge_->CreateSurfaceLayer();

  // TODO(tmathmeyer) does this need support for the reflection transformation
  // as well?
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoFrameCompositor::EnableSubmission,
          base::Unretained(compositor_.get()), bridge_->GetSurfaceId(),
          bridge_->GetLocalSurfaceIdAllocationTime(),
          pipeline_metadata_.video_decoder_config.video_transformation()
              .rotation,
          IsInPictureInPicture()));
  bridge_->SetContentsOpaque(opaque_);

  // If the element is already in Picture-in-Picture mode, it means that it
  // was set in this mode prior to this load, with a different
  // WebMediaPlayerImpl. The new player needs to send its id, size and
  // surface id to the browser process to make sure the states are properly
  // updated.
  // TODO(872056): the surface should be activated but for some reasons, it
  // does not. It is possible that this will no longer be needed after 872056
  // is fixed.
  if (IsInPictureInPicture())
    OnSurfaceIdUpdated(bridge_->GetSurfaceId());
}

void WebMediaPlayerImpl::OnBufferingStateChange(BufferingState state) {
  OnBufferingStateChangeInternal(state, false);
}

void WebMediaPlayerImpl::CreateVideoDecodeStatsReporter() {
  // TODO(chcunningham): destroy reporter if we initially have video but the
  // track gets disabled. Currently not possible in default desktop Chrome.
  if (!HasVideo())
    return;

  // Only record stats from the local pipeline.
  if (is_flinging_ || is_remote_rendering_ || using_media_player_renderer_)
    return;

  // Stats reporter requires a valid config. We may not have one for HLS cases
  // where URL demuxer doesn't know details of the stream.
  if (!pipeline_metadata_.video_decoder_config.IsValidConfig())
    return;

  // Profile must be known for use as index to save the reported stats.
  if (pipeline_metadata_.video_decoder_config.profile() ==
      VIDEO_CODEC_PROFILE_UNKNOWN) {
    return;
  }

  // CdmConfig must be provided for use as index to save encrypted stats.
  if (is_encrypted_ && !cdm_config_) {
    return;
  } else if (cdm_config_) {
    DCHECK(!key_system_.empty());
  }

  mojom::VideoDecodeStatsRecorderPtr recorder;
  media_metrics_provider_->AcquireVideoDecodeStatsRecorder(
      mojo::MakeRequest(&recorder));

  // Create capabilities reporter and synchronize its initial state.
  video_decode_stats_reporter_.reset(new VideoDecodeStatsReporter(
      std::move(recorder),
      base::Bind(&WebMediaPlayerImpl::GetPipelineStatistics,
                 base::Unretained(this)),
      pipeline_metadata_.video_decoder_config.profile(),
      pipeline_metadata_.natural_size, key_system_, cdm_config_,
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia)));

  if (delegate_->IsFrameHidden())
    video_decode_stats_reporter_->OnHidden();
  else
    video_decode_stats_reporter_->OnShown();

  if (paused_)
    video_decode_stats_reporter_->OnPaused();
  else
    video_decode_stats_reporter_->OnPlaying();
}

void WebMediaPlayerImpl::OnProgress() {
  DVLOG(4) << __func__;
  if (highest_ready_state_ < ReadyState::kReadyStateHaveMetadata) {
    // Reset the preroll attempt clock.
    preroll_attempt_pending_ = true;
    preroll_attempt_start_time_ = base::TimeTicks();

    // Clear any 'stale' flag and give the pipeline a chance to resume. If we
    // are already resumed, this will cause |preroll_attempt_start_time_| to
    // be set.
    delegate_->ClearStaleFlag(delegate_id_);
    UpdatePlayState();
  } else if (ready_state_ == ReadyState::kReadyStateHaveFutureData &&
             CanPlayThrough()) {
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
  }
}

bool WebMediaPlayerImpl::CanPlayThrough() {
  if (!base::FeatureList::IsEnabled(kSpecCompliantCanPlayThrough))
    return true;
  if (chunk_demuxer_)
    return true;
  if (data_source_ && data_source_->AssumeFullyBuffered())
    return true;
  // If we're not currently downloading, we have as much buffer as
  // we're ever going to get, which means we say we can play through.
  if (network_state_ == WebMediaPlayer::kNetworkStateIdle)
    return true;
  return buffered_data_source_host_->CanPlayThrough(
      base::TimeDelta::FromSecondsD(CurrentTime()),
      base::TimeDelta::FromSecondsD(Duration()),
      playback_rate_ == 0.0 ? 1.0 : playback_rate_);
}

void WebMediaPlayerImpl::OnBufferingStateChangeInternal(
    BufferingState state,
    bool for_suspended_start) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore buffering state changes caused by back-to-back seeking, so as not
  // to assume the second seek has finished when it was only the first seek.
  if (pipeline_controller_->IsPendingSeek())
    return;

  auto log_event = media_log_->CreateBufferingStateChangedEvent(
      "pipeline_buffering_state", state);
  log_event->params.SetBoolean("for_suspended_start", for_suspended_start);
  media_log_->AddEvent(std::move(log_event));

  if (state == BUFFERING_HAVE_ENOUGH) {
    TRACE_EVENT1("media", "WebMediaPlayerImpl::BufferingHaveEnough", "id",
                 media_log_->id());
    // The SetReadyState() call below may clear
    // |skip_metrics_due_to_startup_suspend_| so report this first.
    if (!have_reported_time_to_play_ready_ &&
        !skip_metrics_due_to_startup_suspend_) {
      DCHECK(!for_suspended_start);
      have_reported_time_to_play_ready_ = true;
      const base::TimeDelta elapsed = base::TimeTicks::Now() - load_start_time_;
      media_metrics_provider_->SetTimeToPlayReady(elapsed);
      RecordTimingUMA("Media.TimeToPlayReady", elapsed);
    }

    // Warning: This call may be re-entrant.
    SetReadyState(CanPlayThrough() ? WebMediaPlayer::kReadyStateHaveEnoughData
                                   : WebMediaPlayer::kReadyStateHaveFutureData);

    // Let the DataSource know we have enough data -- this is the only function
    // during which we advance to (or past) the kReadyStateHaveEnoughData state.
    // It may use this information to update buffer sizes or release unused
    // network connections.
    MaybeUpdateBufferSizesForPlayback();
    if (mb_data_source_ && !client_->CouldPlayIfEnoughData()) {
      // For LazyLoad this will be handled during OnPipelineSuspended().
      if (for_suspended_start && did_lazy_load_)
        DCHECK(!have_enough_after_lazy_load_cb_.IsCancelled());
      else
        mb_data_source_->OnBufferingHaveEnough(false);
    }

    // Blink expects a timeChanged() in response to a seek().
    if (should_notify_time_changed_) {
      should_notify_time_changed_ = false;
      client_->TimeChanged();
    }

    // Once we have enough, start reporting the total memory usage. We'll also
    // report once playback starts.
    ReportMemoryUsage();

    // Report the amount of time it took to leave the underflow state.
    if (underflow_timer_) {
      RecordUnderflowDuration(underflow_timer_->Elapsed());
      underflow_timer_.reset();
    }
  } else {
    // Buffering has underflowed.
    DCHECK_EQ(state, BUFFERING_HAVE_NOTHING);

    // Report the number of times we've entered the underflow state. Ensure we
    // only report the value when transitioning from HAVE_ENOUGH to
    // HAVE_NOTHING.
    if (ready_state_ == WebMediaPlayer::kReadyStateHaveEnoughData &&
        !seeking_) {
      underflow_timer_.reset(new base::ElapsedTimer());
      watch_time_reporter_->OnUnderflow();
    }

    // It shouldn't be possible to underflow if we've not advanced past
    // HAVE_CURRENT_DATA.
    DCHECK_GT(highest_ready_state_, WebMediaPlayer::kReadyStateHaveCurrentData);
    SetReadyState(WebMediaPlayer::kReadyStateHaveCurrentData);
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnDurationChange() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (frame_->IsAdSubframe()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ads.Media.Duration", GetPipelineMediaDuration(),
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromDays(1),
                               50 /* bucket_count */);
  }

  // TODO(sandersd): We should call delegate_->DidPlay() with the new duration,
  // especially if it changed from  <5s to >5s.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return;

  client_->DurationChanged();
  if (watch_time_reporter_)
    watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());
}

void WebMediaPlayerImpl::OnAddTextTrack(const TextTrackConfig& config,
                                        const AddTextTrackDoneCB& done_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const WebInbandTextTrackImpl::Kind web_kind =
      static_cast<WebInbandTextTrackImpl::Kind>(config.kind());
  const blink::WebString web_label = blink::WebString::FromUTF8(config.label());
  const blink::WebString web_language =
      blink::WebString::FromUTF8(config.language());
  const blink::WebString web_id = blink::WebString::FromUTF8(config.id());

  std::unique_ptr<WebInbandTextTrackImpl> web_inband_text_track(
      new WebInbandTextTrackImpl(web_kind, web_label, web_language, web_id));

  std::unique_ptr<TextTrack> text_track(new TextTrackImpl(
      main_task_runner_, client_, std::move(web_inband_text_track)));

  done_cb.Run(std::move(text_track));
}

void WebMediaPlayerImpl::OnWaiting(WaitingReason reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  switch (reason) {
    case WaitingReason::kNoDecryptionKey:
      encrypted_client_->DidBlockPlaybackWaitingForKey();
      // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
      // when a key has been successfully added (e.g. OnSessionKeysChange() with
      // |has_additional_usable_key| = true). http://crbug.com/461903
      encrypted_client_->DidResumePlaybackBlockedForKey();
      return;

    // Ideally this should be handled by PipelineController directly without
    // being proxied here. But currently Pipeline::Client (|this|) is passed to
    // PipelineImpl directly without going through |pipeline_controller_|,
    // making it difficult to do.
    // TODO(xhwang): Handle this in PipelineController when we have a clearer
    // picture on how to refactor WebMediaPlayerImpl, PipelineController and
    // PipelineImpl.
    case WaitingReason::kDecoderStateLost:
      pipeline_controller_->OnDecoderStateLost();
      return;
  }
}

void WebMediaPlayerImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnVideoNaturalSizeChange");

  // The input |size| is from the decoded video frame, which is the original
  // natural size and need to be rotated accordingly.
  gfx::Size rotated_size = GetRotatedVideoSize(
      pipeline_metadata_.video_decoder_config.video_transformation().rotation,
      size);

  RecordVideoNaturalSize(rotated_size);

  gfx::Size old_size = pipeline_metadata_.natural_size;
  if (rotated_size == old_size)
    return;

  pipeline_metadata_.natural_size = rotated_size;

  if (using_media_player_renderer_ && old_size.IsEmpty()) {
    // If we are using MediaPlayerRenderer and this is the first size change, we
    // now know that there is a video track. This condition is paired with code
    // in CreateWatchTimeReporter() that guesses the existence of a video track.
    CreateWatchTimeReporter();
  } else {
    UpdateSecondaryProperties();
  }

  if (video_decode_stats_reporter_ &&
      !video_decode_stats_reporter_->MatchesBucketedNaturalSize(
          pipeline_metadata_.natural_size)) {
    CreateVideoDecodeStatsReporter();
  }

  client_->SizeChanged();

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  delegate_->DidPlayerSizeChange(delegate_id_, NaturalSize());
}

void WebMediaPlayerImpl::OnVideoOpacityChange(bool opaque) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  opaque_ = opaque;
  if (!surface_layer_for_video_enabled_ && video_layer_)
    video_layer_->SetContentsOpaque(opaque_);
  else if (bridge_->GetCcLayer())
    bridge_->SetContentsOpaque(opaque_);
}

void WebMediaPlayerImpl::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  const bool codec_change =
      pipeline_metadata_.audio_decoder_config.codec() != config.codec();
  pipeline_metadata_.audio_decoder_config = config;

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  if (codec_change)
    UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  const bool codec_change =
      pipeline_metadata_.video_decoder_config.codec() != config.codec();
  const bool codec_profile_change =
      pipeline_metadata_.video_decoder_config.profile() != config.profile();

  pipeline_metadata_.video_decoder_config = config;

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  if (codec_change)
    UpdateSecondaryProperties();

  if (video_decode_stats_reporter_ && codec_profile_change)
    CreateVideoDecodeStatsReporter();
}

void WebMediaPlayerImpl::OnVideoAverageKeyframeDistanceUpdate() {
  UpdateBackgroundVideoOptimizationState();
}

void WebMediaPlayerImpl::OnAudioDecoderChange(const std::string& name) {
  if (name == audio_decoder_name_)
    return;

  audio_decoder_name_ = name;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnVideoDecoderChange(const std::string& name) {
  if (name == video_decoder_name_)
    return;

  video_decoder_name_ = name;

  // If there's no current reporter, there's nothing to be done.
  if (!watch_time_reporter_)
    return;

  UpdateSecondaryProperties();
}

void WebMediaPlayerImpl::OnFrameHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Backgrounding a video requires a user gesture to resume playback.
  if (IsHidden())
    video_locked_when_paused_when_hidden_ = true;

  if (watch_time_reporter_)
    watch_time_reporter_->OnHidden();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnHidden();

  UpdateBackgroundVideoOptimizationState();
  UpdatePlayState();

  // Schedule suspended playing media to be paused if the user doesn't come back
  // to it within some timeout period to avoid any autoplay surprises.
  ScheduleIdlePauseTimer();

  // Notify the compositor of our page visibility status.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetIsPageVisible,
                     base::Unretained(compositor_.get()), !IsHidden()));
}

void WebMediaPlayerImpl::OnFrameClosed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnFrameShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  background_pause_timer_.Stop();

  // Foreground videos don't require user gesture to continue playback.
  video_locked_when_paused_when_hidden_ = false;

  if (watch_time_reporter_)
    watch_time_reporter_->OnShown();

  if (video_decode_stats_reporter_)
    video_decode_stats_reporter_->OnShown();

  // Notify the compositor of our page visibility status.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetIsPageVisible,
                     base::Unretained(compositor_.get()), !IsHidden()));

  UpdateBackgroundVideoOptimizationState();

  if (paused_when_hidden_) {
    paused_when_hidden_ = false;
    OnPlay();  // Calls UpdatePlayState() so return afterwards.
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnIdleTimeout() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // This should never be called when stale state testing overrides are used.
  DCHECK(!stale_state_override_for_testing_.has_value());

  // If we are attempting preroll, clear the stale flag.
  if (IsPrerollAttemptNeeded()) {
    delegate_->ClearStaleFlag(delegate_id_);
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnPlay() {
  client_->RequestPlay();
}

void WebMediaPlayerImpl::OnPause() {
  client_->RequestPause();
}

void WebMediaPlayerImpl::OnMuted(bool muted) {
  client_->RequestMuted(muted);
}

void WebMediaPlayerImpl::OnSeekForward(double seconds) {
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  client_->RequestSeek(CurrentTime() + seconds);
}

void WebMediaPlayerImpl::OnSeekBackward(double seconds) {
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  client_->RequestSeek(CurrentTime() - seconds);
}

void WebMediaPlayerImpl::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  SetVolume(volume_);
}

void WebMediaPlayerImpl::OnBecamePersistentVideo(bool value) {
  client_->OnBecamePersistentVideo(value);
  overlay_info_.is_persistent_video = value;
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::SendBytesReceivedUpdate() {
  media_metrics_provider_->AddBytesReceived(bytes_received_since_last_update_);
  bytes_received_since_last_update_ = 0;
}

void WebMediaPlayerImpl::OnBytesReceived(uint64_t data_length) {
  bytes_received_since_last_update_ += data_length;
  constexpr base::TimeDelta kBytesReceivedUpdateInterval =
      base::TimeDelta::FromMilliseconds(500);
  auto current_time = base::TimeTicks::Now();
  if (earliest_time_next_bytes_received_update_.is_null() ||
      earliest_time_next_bytes_received_update_ <= current_time) {
    report_bytes_received_timer_.Stop();
    SendBytesReceivedUpdate();
    earliest_time_next_bytes_received_update_ =
        current_time + kBytesReceivedUpdateInterval;
  } else {
    report_bytes_received_timer_.Start(
        FROM_HERE, kBytesReceivedUpdateInterval, this,
        &WebMediaPlayerImpl::SendBytesReceivedUpdate);
  }
}

void WebMediaPlayerImpl::ScheduleRestart() {
  // TODO(watk): All restart logic should be moved into PipelineController.
  if (pipeline_controller_->IsPipelineRunning() &&
      !pipeline_controller_->IsPipelineSuspended()) {
    pending_suspend_resume_cycle_ = true;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::RequestRemotePlaybackDisabled(bool disabled) {
  if (observer_)
    observer_->OnRemotePlaybackDisabled(disabled);
}

#if defined(OS_ANDROID)
void WebMediaPlayerImpl::FlingingStarted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = true;

  is_flinging_ = true;

  // Capabilities reporting should only be performed for local playbacks.
  video_decode_stats_reporter_.reset();

  // Requests to restart media pipeline. A flinging renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
}

void WebMediaPlayerImpl::FlingingStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = false;

  is_flinging_ = false;

  CreateVideoDecodeStatsReporter();

  ScheduleRestart();
}

void WebMediaPlayerImpl::OnRemotePlayStateChange(MediaStatus::State state) {
  DCHECK(is_flinging_);

  if (state == MediaStatus::State::PLAYING && Paused()) {
    DVLOG(1) << __func__ << " requesting PLAY.";
    client_->RequestPlay();
  } else if (state == MediaStatus::State::PAUSED && !Paused()) {
    DVLOG(1) << __func__ << " requesting PAUSE.";
    client_->RequestPause();
  }
}
#endif  // defined(OS_ANDROID)

void WebMediaPlayerImpl::SetPoster(const blink::WebURL& poster) {
  has_poster_ = !poster.IsEmpty();
}

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (observer_ && mb_data_source_)
    observer_->OnDataSourceInitialized(mb_data_source_->GetUrlAfterRedirects());

  if (!success) {
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
    media_metrics_provider_->OnError(PIPELINE_ERROR_NETWORK);

    // Not really necessary, since the pipeline was never started, but it at
    // least this makes sure that the error handling code is in sync.
    UpdatePlayState();

    return;
  }

  // No point in preloading data as we'll probably just throw it away anyways.
  if (IsStreaming() && preload_ > MultibufferDataSource::METADATA &&
      mb_data_source_) {
    mb_data_source_->SetPreload(MultibufferDataSource::METADATA);
  }

  StartPipeline();
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  DVLOG(1) << __func__ << "(" << is_downloading << ")";
  if (!is_downloading && network_state_ == WebMediaPlayer::kNetworkStateLoading)
    SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  else if (is_downloading &&
           network_state_ == WebMediaPlayer::kNetworkStateIdle)
    SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  if (ready_state_ == ReadyState::kReadyStateHaveFutureData && !is_downloading)
    SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
}

void WebMediaPlayerImpl::OnOverlayRoutingToken(
    const base::UnguessableToken& token) {
  DCHECK(overlay_mode_ == OverlayMode::kUseAndroidOverlay);
  // TODO(liberato): |token| should already be a RoutingToken.
  overlay_routing_token_is_pending_ = false;
  overlay_routing_token_ = OverlayInfo::RoutingToken(token);
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::OnOverlayInfoRequested(
    bool decoder_requires_restart_for_overlay,
    const ProvideOverlayInfoCB& provide_overlay_info_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // If we get a non-null cb, a decoder is initializing and requires overlay
  // info. If we get a null cb, a previously initialized decoder is
  // unregistering for overlay info updates.
  if (!provide_overlay_info_cb) {
    decoder_requires_restart_for_overlay_ = false;
    provide_overlay_info_cb_.Reset();
    return;
  }

  // If |decoder_requires_restart_for_overlay| is true, we must restart the
  // pipeline for fullscreen transitions. The decoder is unable to switch
  // surfaces otherwise. If false, we simply need to tell the decoder about the
  // new surface and it will handle things seamlessly.
  // For encrypted video we pretend that the decoder doesn't require a restart
  // because it needs an overlay all the time anyway. We'll switch into
  // |always_enable_overlays_| mode below.
  decoder_requires_restart_for_overlay_ =
      (overlay_mode_ == OverlayMode::kUseAndroidOverlay && is_encrypted_)
          ? false
          : decoder_requires_restart_for_overlay;
  provide_overlay_info_cb_ = provide_overlay_info_cb;

  // If the decoder doesn't require restarts for surface transitions, and we're
  // using AndroidOverlay mode, we can always enable the overlay and the decoder
  // can choose whether or not to use it. Otherwise, we'll restart the decoder
  // and enable the overlay on fullscreen transitions.
  if (overlay_mode_ == OverlayMode::kUseAndroidOverlay &&
      !decoder_requires_restart_for_overlay_) {
    always_enable_overlays_ = true;
    if (!overlay_enabled_)
      EnableOverlay();
  }

  // Send the overlay info if we already have it. If not, it will be sent later.
  MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::MaybeSendOverlayInfoToDecoder() {
  // If the decoder didn't request overlay info, then don't send it.
  if (!provide_overlay_info_cb_)
    return;

  // We should send the overlay info as long as we know it.  This includes the
  // case where |!overlay_enabled_|, since we want to tell the decoder to avoid
  // using overlays.  Assuming that the decoder has requested info, the only
  // case in which we don't want to send something is if we've requested the
  // info but not received it yet.  Then, we should wait until we do.
  //
  // Initialization requires this; AVDA should start with enough info to make an
  // overlay, so that (pre-M) the initial codec is created with the right output
  // surface; it can't switch later.
  if (overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    if (overlay_routing_token_is_pending_)
      return;

    overlay_info_.routing_token = overlay_routing_token_;
  }

  // If restart is required, the callback is one-shot only.
  if (decoder_requires_restart_for_overlay_) {
    std::move(provide_overlay_info_cb_).Run(overlay_info_);
  } else {
    provide_overlay_info_cb_.Run(overlay_info_);
  }
}

std::unique_ptr<Renderer> WebMediaPlayerImpl::CreateRenderer() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Make sure that overlays are enabled if they're always allowed.
  if (always_enable_overlays_)
    EnableOverlay();

  RequestOverlayInfoCB request_overlay_info_cb;
#if defined(OS_ANDROID)
  request_overlay_info_cb = BindToCurrentLoop(
      base::Bind(&WebMediaPlayerImpl::OnOverlayInfoRequested, weak_this_));
#endif
  return renderer_factory_selector_->GetCurrentFactory()->CreateRenderer(
      media_task_runner_, worker_task_runner_, audio_source_provider_.get(),
      compositor_.get(), request_overlay_info_cb, client_->TargetColorSpace());
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      BindToCurrentLoop(base::Bind(
          &WebMediaPlayerImpl::OnEncryptedMediaInitData, weak_this_));

  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::SetOnNewProcessedFrameCallback,
                     base::Unretained(compositor_.get()),
                     BindToCurrentLoop(base::BindOnce(
                         &WebMediaPlayerImpl::OnFirstFrame, weak_this_))));

#if defined(OS_ANDROID)
  if (demuxer_found_hls_ ||
      renderer_factory_selector_->GetCurrentFactory()
              ->GetRequiredMediaResourceType() == MediaResource::Type::URL) {
    // MediaPlayerRendererClientFactory is the only factory that a uses
    // MediaResource::Type::URL for the moment.
    using_media_player_renderer_ = true;

    // MediaPlayerRenderer does not provide pipeline stats, so nuke capabilities
    // reporter.
    video_decode_stats_reporter_.reset();

    demuxer_.reset(new MediaUrlDemuxer(
        media_task_runner_, loaded_url_, frame_->GetDocument().SiteForCookies(),
        allow_media_player_renderer_credentials_));
    pipeline_controller_->Start(Pipeline::StartType::kNormal, demuxer_.get(),
                                this, false, false);
    return;
  }
#endif  // defined(OS_ANDROID)

  // Figure out which demuxer to use.
  if (load_type_ != kLoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

#if BUILDFLAG(ENABLE_FFMPEG)
    Demuxer::MediaTracksUpdatedCB media_tracks_updated_cb =
        BindToCurrentLoop(base::Bind(
            &WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated, weak_this_));

    demuxer_.reset(new FFmpegDemuxer(
        media_task_runner_, data_source_.get(), encrypted_media_init_data_cb,
        media_tracks_updated_cb, media_log_.get(), IsLocalFile(loaded_url_)));
#else
    OnError(PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
#endif
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    chunk_demuxer_ =
        new ChunkDemuxer(BindToCurrentLoop(base::Bind(
                             &WebMediaPlayerImpl::OnDemuxerOpened, weak_this_)),
                         BindToCurrentLoop(base::Bind(
                             &WebMediaPlayerImpl::OnProgress, weak_this_)),
                         encrypted_media_init_data_cb, media_log_.get());
    // Notify |this| of bytes that are received via MSE.
    chunk_demuxer_->AddBytesReceivedCallback(BindToCurrentLoop(
        base::BindRepeating(&WebMediaPlayerImpl::OnBytesReceived, weak_this_)));
    demuxer_.reset(chunk_demuxer_);

    if (base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC)) {
      // base::Unretained is safe because |this| owns memory_pressure_listener_.
      memory_pressure_listener_ =
          std::make_unique<base::MemoryPressureListener>(base::Bind(
              &WebMediaPlayerImpl::OnMemoryPressure, base::Unretained(this)));
    }
  }

  // TODO(sandersd): FileSystem objects may also be non-static, but due to our
  // caching layer such situations are broken already. http://crbug.com/593159
  bool is_static = !chunk_demuxer_;
  bool is_streaming = IsStreaming();
  UMA_HISTOGRAM_BOOLEAN("Media.IsStreaming", is_streaming);

  // If possible attempt to avoid decoder spool up until playback starts.
  Pipeline::StartType start_type = Pipeline::StartType::kNormal;
  if (!chunk_demuxer_ && preload_ == MultibufferDataSource::METADATA &&
      !client_->CouldPlayIfEnoughData() && !IsStreaming()) {
    start_type =
        (has_poster_ || base::FeatureList::IsEnabled(kPreloadMetadataLazyLoad))
            ? Pipeline::StartType::kSuspendAfterMetadata
            : Pipeline::StartType::kSuspendAfterMetadataForAudioOnly;
    attempting_suspended_start_ = true;
  }

  // ... and we're ready to go!
  // TODO(sandersd): On Android, defer Start() if the tab is not visible.
  seeking_ = true;
  pipeline_controller_->Start(start_type, demuxer_.get(), this, is_streaming,
                              is_static);
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->NetworkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (state == WebMediaPlayer::kReadyStateHaveEnoughData && data_source_ &&
      data_source_->AssumeFullyBuffered() &&
      network_state_ == WebMediaPlayer::kNetworkStateLoading) {
    SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);
  }

  ready_state_ = state;
  highest_ready_state_ = std::max(highest_ready_state_, ready_state_);

  // Always notify to ensure client has the latest value.
  client_->ReadyStateChanged();
}

scoped_refptr<blink::WebAudioSourceProviderImpl>
WebMediaPlayerImpl::GetAudioSourceProvider() {
  return audio_source_provider_;
}

scoped_refptr<VideoFrame> WebMediaPlayerImpl::GetCurrentFrameFromCompositor()
    const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");

  // Can be null.
  scoped_refptr<VideoFrame> video_frame =
      compositor_->GetCurrentFrameOnAnyThread();

  // base::Unretained is safe here because |compositor_| is destroyed on
  // |vfc_task_runner_|. The destruction is queued from |this|' destructor,
  // which also runs on |main_task_runner_|, which makes it impossible for
  // UpdateCurrentFrameIfStale() to be queued after |compositor_|'s dtor.
  vfc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameCompositor::UpdateCurrentFrameIfStale,
                     base::Unretained(compositor_.get())));

  return video_frame;
}

void WebMediaPlayerImpl::UpdatePlayState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool can_auto_suspend = !disable_pipeline_auto_suspend_;
  // For streaming videos, we only allow suspending at the very beginning of the
  // video, and only if we know the length of the video. (If we don't know
  // the length, it might be a dynamically generated video, and suspending
  // will not work at all.)
  if (IsStreaming()) {
    bool at_beginning =
        ready_state_ == WebMediaPlayer::kReadyStateHaveNothing ||
        CurrentTime() == 0.0;
    if (!at_beginning || GetPipelineMediaDuration() == kInfiniteDuration)
      can_auto_suspend = false;
  }

  bool is_suspended = pipeline_controller_->IsSuspended();
  bool is_backgrounded = IsBackgroundSuspendEnabled(this) && IsHidden();
  PlayState state = UpdatePlayState_ComputePlayState(
      is_flinging_, can_auto_suspend, is_suspended, is_backgrounded);
  SetDelegateState(state.delegate_state, state.is_idle);
  SetMemoryReportingState(state.is_memory_reporting_enabled);
  SetSuspendState(state.is_suspended || pending_suspend_resume_cycle_);
}

void WebMediaPlayerImpl::SetDelegateState(DelegateState new_state,
                                          bool is_idle) {
  DCHECK(delegate_);
  DVLOG(2) << __func__ << "(" << static_cast<int>(new_state) << ", " << is_idle
           << ")";

  // Prevent duplicate delegate calls.
  // TODO(sandersd): Move this deduplication into the delegate itself.
  // TODO(sandersd): WebContentsObserverSanityChecker does not allow sending the
  // 'playing' IPC more than once in a row, even if the metadata has changed.
  // Figure out whether it should.
  // Pretend that the media has no audio if it never played unmuted. This is to
  // avoid any action related to audible media such as taking audio focus or
  // showing a media notification. To preserve a consistent experience, it does
  // not apply if a media was audible so the system states do not flicker
  // depending on whether the user muted the player.
  bool has_audio = HasAudio() && !client_->WasAlwaysMuted();
  if (delegate_state_ == new_state &&
      (delegate_state_ != DelegateState::PLAYING ||
       delegate_has_audio_ == has_audio)) {
    return;
  }
  delegate_state_ = new_state;
  delegate_has_audio_ = has_audio;

  switch (new_state) {
    case DelegateState::GONE:
      delegate_->PlayerGone(delegate_id_);
      break;
    case DelegateState::PLAYING: {
      if (HasVideo())
        delegate_->DidPlayerSizeChange(delegate_id_, NaturalSize());
      delegate_->DidPlay(
          delegate_id_, HasVideo(), has_audio,
          DurationToMediaContentType(GetPipelineMediaDuration()));
      break;
    }
    case DelegateState::PAUSED:
      delegate_->DidPause(delegate_id_);
      break;
  }

  delegate_->SetIdle(delegate_id_, is_idle);
}

void WebMediaPlayerImpl::SetMemoryReportingState(
    bool is_memory_reporting_enabled) {
  if (memory_usage_reporting_timer_.IsRunning() ==
      is_memory_reporting_enabled) {
    return;
  }

  if (is_memory_reporting_enabled) {
    memory_usage_reporting_timer_.Start(FROM_HERE,
                                        base::TimeDelta::FromSeconds(2), this,
                                        &WebMediaPlayerImpl::ReportMemoryUsage);
  } else {
    memory_usage_reporting_timer_.Stop();
    ReportMemoryUsage();
  }
}

void WebMediaPlayerImpl::SetSuspendState(bool is_suspended) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__ << "(" << is_suspended << ")";

  // Do not change the state after an error has occurred.
  // TODO(sandersd): Update PipelineController to remove the need for this.
  if (IsNetworkStateError(network_state_))
    return;

  if (is_suspended) {
    // If we were not resumed for long enough to satisfy the preroll attempt,
    // reset the clock.
    if (!preroll_attempt_pending_ && IsPrerollAttemptNeeded()) {
      preroll_attempt_pending_ = true;
      preroll_attempt_start_time_ = base::TimeTicks();
    }
    pipeline_controller_->Suspend();
  } else {
    // When resuming, start the preroll attempt clock.
    if (preroll_attempt_pending_) {
      preroll_attempt_pending_ = false;
      preroll_attempt_start_time_ = tick_clock_->NowTicks();
    }
    pipeline_controller_->Resume();
  }
}

WebMediaPlayerImpl::PlayState
WebMediaPlayerImpl::UpdatePlayState_ComputePlayState(bool is_flinging,
                                                     bool can_auto_suspend,
                                                     bool is_suspended,
                                                     bool is_backgrounded) {
  PlayState result;

  bool must_suspend = delegate_->IsFrameClosed();
  bool is_stale = delegate_->IsStale(delegate_id_);

  if (stale_state_override_for_testing_.has_value() &&
      ready_state_ >= stale_state_override_for_testing_.value()) {
    is_stale = true;
  }

  // This includes both data source (before pipeline startup) and pipeline
  // errors.
  bool has_error = IsNetworkStateError(network_state_);

  // After kReadyStateHaveMetadata, Blink will call play() if the state is not
  // paused; prior to this point |paused_| is not accurate.
  bool have_metadata =
      highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata;

  // Background suspend is only enabled for paused players.
  // In the case of players with audio the session should be kept.
  bool background_suspended =
      can_auto_suspend && is_backgrounded && paused_ && have_metadata;

  // Idle suspension is allowed prior to kReadyStateHaveMetadata since there
  // exist mechanisms to exit the idle state when the player is capable of
  // reaching the kReadyStateHaveMetadata state; see didLoadingProgress().
  //
  // TODO(sandersd): Make the delegate suspend idle players immediately when
  // hidden.
  bool idle_suspended = can_auto_suspend && is_stale && paused_ && !seeking_ &&
                        !overlay_enabled_ && !needs_first_frame_;

  // If we're already suspended, see if we can wait for user interaction. Prior
  // to kReadyStateHaveMetadata, we require |is_stale| to remain suspended.
  // |is_stale| will be cleared when we receive data which may take us to
  // kReadyStateHaveMetadata.
  bool can_stay_suspended = (is_stale || have_metadata) && is_suspended &&
                            paused_ && !seeking_ && !needs_first_frame_;

  // Combined suspend state.
  result.is_suspended = must_suspend || idle_suspended ||
                        background_suspended || can_stay_suspended;

  DVLOG(3) << __func__ << ": must_suspend=" << must_suspend
           << ", idle_suspended=" << idle_suspended
           << ", background_suspended=" << background_suspended
           << ", can_stay_suspended=" << can_stay_suspended
           << ", is_stale=" << is_stale << ", have_metadata=" << have_metadata
           << ", paused_=" << paused_ << ", seeking_=" << seeking_;

  // We do not treat |playback_rate_| == 0 as paused. For the media session,
  // being paused implies displaying a play button, which is incorrect in this
  // case. For memory usage reporting, we just use the same definition (but we
  // don't have to).
  //
  // Similarly, we don't consider |ended_| to be paused. Blink will immediately
  // call pause() or seek(), so |ended_| should not affect the computation.
  // Despite that, |ended_| does result in a separate paused state, to simplfy
  // the contract for SetDelegateState().
  //
  // |has_remote_controls| indicates if the player can be controlled outside the
  // page (e.g. via the notification controls or by audio focus events). Idle
  // suspension does not destroy the media session, because we expect that the
  // notification controls (and audio focus) remain. With some exceptions for
  // background videos, the player only needs to have audio to have controls
  // (requires |have_current_data|).
  //
  // |alive| indicates if the player should be present (not |GONE|) to the
  // delegate, either paused or playing. The following must be true for the
  // player:
  //   - |have_current_data|, since playback can't begin before that point, we
  //     need to know whether we are paused to correctly configure the session,
  //     and also because the tracks and duration are passed to DidPlay(),
  //   - |is_flinging| is false (RemotePlayback is not handled by the delegate)
  //   - |has_error| is false as player should have no errors,
  //   - |background_suspended| is false, otherwise |has_remote_controls| must
  //     be true.
  //
  // TODO(sandersd): If Blink told us the paused state sooner, we could detect
  // if the remote controls are available sooner.

  // Background videos with audio don't have remote controls if background
  // suspend is enabled and resuming background videos is not (original Android
  // behavior).
  bool backgrounded_video_has_no_remote_controls =
      IsBackgroundSuspendEnabled(this) && !IsResumeBackgroundVideosEnabled() &&
      is_backgrounded && HasVideo();
  bool have_current_data = highest_ready_state_ >= kReadyStateHaveCurrentData;
  bool can_play = !has_error && have_current_data;
  bool has_remote_controls =
      HasAudio() && !backgrounded_video_has_no_remote_controls;
  bool alive = can_play && !is_flinging && !must_suspend &&
               (!background_suspended || has_remote_controls);
  if (!alive) {
    // Do not mark players as idle when flinging.
    result.delegate_state = DelegateState::GONE;
    result.is_idle = delegate_->IsIdle(delegate_id_) && !is_flinging;
  } else if (paused_) {
    // TODO(sandersd): Is it possible to have a suspended session, be ended,
    // and not be paused? If so we should be in a PLAYING state.
    result.delegate_state =
        ended_ ? DelegateState::GONE : DelegateState::PAUSED;
    result.is_idle = !seeking_;
  } else {
    result.delegate_state = DelegateState::PLAYING;
    result.is_idle = false;
  }

  // It's not critical if some cases where memory usage can change are missed,
  // since media memory changes are usually gradual.
  result.is_memory_reporting_enabled = !has_error && can_play && !is_flinging &&
                                       !result.is_suspended &&
                                       (!paused_ || seeking_);

  return result;
}

void WebMediaPlayerImpl::ReportMemoryUsage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // About base::Unretained() usage below: We destroy |demuxer_| on the main
  // thread.  Before that, however, ~WebMediaPlayerImpl() posts a task to the
  // media thread and waits for it to finish.  Hence, the GetMemoryUsage() task
  // posted here must finish earlier.
  //
  // The exception to the above is when OnError() has been called. If we're in
  // the error state we've already shut down the pipeline and can't rely on it
  // to cycle the media thread before we destroy |demuxer_|. In this case skip
  // collection of the demuxer memory stats.
  if (demuxer_ && !IsNetworkStateError(network_state_)) {
    base::PostTaskAndReplyWithResult(
        media_task_runner_.get(), FROM_HERE,
        base::Bind(&Demuxer::GetMemoryUsage, base::Unretained(demuxer_.get())),
        base::Bind(&WebMediaPlayerImpl::FinishMemoryUsageReport, weak_this_));
  } else {
    FinishMemoryUsageReport(0);
  }
}

void WebMediaPlayerImpl::FinishMemoryUsageReport(int64_t demuxer_memory_usage) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const PipelineStatistics stats = GetPipelineStatistics();
  const int64_t data_source_memory_usage =
      data_source_ ? data_source_->GetMemoryUsage() : 0;

  // If we have video and no video memory usage and we've rendered the first
  // frame, assume the VideoFrameCompositor is holding onto the last frame after
  // we've suspended the pipeline; which thus reports zero memory usage from the
  // video renderer.
  //
  // Technically this should use the coded size, but that requires us to hop to
  // the compositor to get and byte-perfect accuracy isn't important here.
  const int64_t video_memory_usage =
      stats.video_memory_usage +
      ((pipeline_metadata_.has_video && !stats.video_memory_usage &&
        has_first_frame_)
           ? VideoFrame::AllocationSize(PIXEL_FORMAT_I420,
                                        pipeline_metadata_.natural_size)
           : 0);

  const int64_t current_memory_usage =
      stats.audio_memory_usage + video_memory_usage + data_source_memory_usage +
      demuxer_memory_usage;

  DVLOG(2) << "Memory Usage -- Total: " << current_memory_usage
           << " Audio: " << stats.audio_memory_usage
           << ", Video: " << video_memory_usage
           << ", DataSource: " << data_source_memory_usage
           << ", Demuxer: " << demuxer_memory_usage;

  const int64_t delta = current_memory_usage - last_reported_memory_usage_;
  last_reported_memory_usage_ = current_memory_usage;
  adjust_allocated_memory_cb_.Run(delta);

  if (HasAudio()) {
    UMA_HISTOGRAM_MEMORY_KB("Media.WebMediaPlayerImpl.Memory.Audio",
                            stats.audio_memory_usage / 1024);
  }
  if (HasVideo()) {
    UMA_HISTOGRAM_MEMORY_KB("Media.WebMediaPlayerImpl.Memory.Video",
                            video_memory_usage / 1024);
  }
  if (data_source_) {
    UMA_HISTOGRAM_MEMORY_KB("Media.WebMediaPlayerImpl.Memory.DataSource",
                            data_source_memory_usage / 1024);
  }
  if (demuxer_) {
    UMA_HISTOGRAM_MEMORY_KB("Media.WebMediaPlayerImpl.Memory.Demuxer",
                            demuxer_memory_usage / 1024);
  }
}

void WebMediaPlayerImpl::ScheduleIdlePauseTimer() {
  // Only schedule the pause timer if we're not paused or paused but going to
  // resume when foregrounded, and are suspended and have audio.
  if ((paused_ && !paused_when_hidden_) ||
      !pipeline_controller_->IsSuspended() || !HasAudio()) {
    return;
  }

#if defined(OS_ANDROID)
  // Don't pause videos casted as part of RemotePlayback.
  if (is_flinging_)
    return;
#endif

  // Idle timeout chosen arbitrarily.
  background_pause_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                                this, &WebMediaPlayerImpl::OnPause);
}

void WebMediaPlayerImpl::CreateWatchTimeReporter() {
  if (!HasVideo() && !HasAudio())
    return;

  // MediaPlayerRenderer does not know about tracks until playback starts.
  // Assume audio-only unless the natural size has been detected.
  bool has_video = pipeline_metadata_.has_video;
  if (using_media_player_renderer_) {
    has_video = !pipeline_metadata_.natural_size.IsEmpty();
  }

  // Create the watch time reporter and synchronize its initial state.
  watch_time_reporter_.reset(new WatchTimeReporter(
      mojom::PlaybackProperties::New(
          pipeline_metadata_.has_audio, has_video, false, false,
          !!chunk_demuxer_, is_encrypted_, embedded_media_experience_enabled_),
      pipeline_metadata_.natural_size,
      base::BindRepeating(&WebMediaPlayerImpl::GetCurrentTimeInternal,
                          base::Unretained(this)),
      media_metrics_provider_.get(),
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia)));
  watch_time_reporter_->OnVolumeChange(volume_);
  watch_time_reporter_->OnDurationChanged(GetPipelineMediaDuration());

  if (delegate_->IsFrameHidden())
    watch_time_reporter_->OnHidden();
  else
    watch_time_reporter_->OnShown();

  if (client_->HasNativeControls())
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();

  switch (client_->DisplayType()) {
    case WebMediaPlayer::DisplayType::kInline:
      watch_time_reporter_->OnDisplayTypeInline();
      break;
    case WebMediaPlayer::DisplayType::kFullscreen:
      watch_time_reporter_->OnDisplayTypeFullscreen();
      break;
    case WebMediaPlayer::DisplayType::kPictureInPicture:
      watch_time_reporter_->OnDisplayTypePictureInPicture();
      break;
  }

  UpdateSecondaryProperties();

  // If the WatchTimeReporter was recreated in the middle of playback, we want
  // to resume playback here too since we won't get another play() call. When
  // seeking, the seek completion will restart it if necessary.
  if (!paused_ && !seeking_)
    watch_time_reporter_->OnPlaying();
}

void WebMediaPlayerImpl::UpdateSecondaryProperties() {
  watch_time_reporter_->UpdateSecondaryProperties(
      mojom::SecondaryPlaybackProperties::New(
          pipeline_metadata_.audio_decoder_config.codec(),
          pipeline_metadata_.video_decoder_config.codec(), audio_decoder_name_,
          video_decoder_name_,
          DetermineEncryptionMode(
              pipeline_metadata_.audio_decoder_config.encryption_scheme()),
          DetermineEncryptionMode(
              pipeline_metadata_.video_decoder_config.encryption_scheme()),
          pipeline_metadata_.natural_size));
}

bool WebMediaPlayerImpl::IsHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return delegate_->IsFrameHidden() && !delegate_->IsFrameClosed();
}

bool WebMediaPlayerImpl::IsStreaming() const {
  return data_source_ && data_source_->IsStreaming();
}

bool WebMediaPlayerImpl::DoesOverlaySupportMetadata() const {
  return pipeline_metadata_.video_decoder_config.video_transformation() ==
         kNoTransformation;
}

void WebMediaPlayerImpl::ActivateViewportIntersectionMonitoring(bool activate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  client_->ActivateViewportIntersectionMonitoring(activate);
}

void WebMediaPlayerImpl::UpdateRemotePlaybackCompatibility(bool is_compatible) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  client_->RemotePlaybackCompatibilityChanged(loaded_url_, is_compatible);
}

void WebMediaPlayerImpl::ForceStaleStateForTesting(ReadyState target_state) {
  stale_state_override_for_testing_.emplace(target_state);
  UpdatePlayState();
}

bool WebMediaPlayerImpl::IsSuspendedForTesting() {
  // This intentionally uses IsPipelineSuspended since we need to know when the
  // pipeline has reached the suspended state, not when it's in suspending.
  return pipeline_controller_->IsPipelineSuspended();
}

bool WebMediaPlayerImpl::DidLazyLoad() const {
  return did_lazy_load_;
}

void WebMediaPlayerImpl::OnBecameVisible() {
  have_enough_after_lazy_load_cb_.Cancel();
  needs_first_frame_ = !has_first_frame_;
  UpdatePlayState();
}

bool WebMediaPlayerImpl::IsOpaque() const {
  return opaque_;
}

int WebMediaPlayerImpl::GetDelegateId() {
  return delegate_id_;
}

base::Optional<viz::SurfaceId> WebMediaPlayerImpl::GetSurfaceId() {
  if (!surface_layer_for_video_enabled_)
    return base::nullopt;
  return bridge_->GetSurfaceId();
}

base::WeakPtr<blink::WebMediaPlayer> WebMediaPlayerImpl::AsWeakPtr() {
  return weak_this_;
}

bool WebMediaPlayerImpl::ShouldPausePlaybackWhenHidden() const {
  // Audio only stream is allowed to play when in background.
  // TODO: We should check IsBackgroundOptimizationCandidate here. But we need
  // to move the logic of checking video frames out of that function.
  if (!HasVideo())
    return false;

  if (!is_background_video_playback_enabled_)
    return true;

  // If suspending background video, pause any video that's not remoted or
  // not unlocked to play in the background.
  if (IsBackgroundSuspendEnabled(this)) {
#if defined(OS_ANDROID)
    if (is_flinging_)
      return false;
#endif

    return !HasAudio() || (IsResumeBackgroundVideosEnabled() &&
                           video_locked_when_paused_when_hidden_);
  }

  // Otherwise only pause if the optimization is on and it's a video-only
  // optimization candidate.
  return IsBackgroundVideoPauseOptimizationEnabled() && !HasAudio() &&
         IsBackgroundOptimizationCandidate() && !is_flinging_;
}

bool WebMediaPlayerImpl::ShouldDisableVideoWhenHidden() const {
  // This optimization is behind the flag on all platforms, only for non-mse
  // video. MSE video track switching on hide has gone through a field test.
  // TODO(tmathmeyer): Passing load_type_ won't be needed after src= field
  // testing is finished. see: http://crbug.com/709302
  if (!is_background_video_track_optimization_supported_)
    return false;

  // Disable video track only for players with audio that match the criteria for
  // being optimized.
  return HasAudio() && IsBackgroundOptimizationCandidate();
}

bool WebMediaPlayerImpl::IsBackgroundOptimizationCandidate() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Don't optimize Picture-in-Picture players.
  if (IsInPictureInPicture())
    return false;

#if defined(OS_ANDROID)
  // Don't optimize videos casted as part of RemotePlayback.
  if (is_flinging_)
    return false;
#endif

  // Don't optimize audio-only or streaming players.
  if (!HasVideo() || IsStreaming())
    return false;

  // Video-only players are always optimized (paused).
  // Don't check the keyframe distance and duration.
  if (!HasAudio() && HasVideo())
    return true;

  // Videos shorter than the maximum allowed keyframe distance can be optimized.
  base::TimeDelta duration = GetPipelineMediaDuration();

  constexpr base::TimeDelta kMaxKeyframeDistanceToDisableBackgroundVideo =
      base::TimeDelta::FromMilliseconds(
          kMaxKeyframeDistanceToDisableBackgroundVideoMs);
  if (duration < kMaxKeyframeDistanceToDisableBackgroundVideo)
    return true;

  // Otherwise, only optimize videos with shorter average keyframe distance.
  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_keyframe_distance_average <
         kMaxKeyframeDistanceToDisableBackgroundVideo;
}

void WebMediaPlayerImpl::UpdateBackgroundVideoOptimizationState() {
  if (IsHidden()) {
    if (ShouldPausePlaybackWhenHidden()) {
      PauseVideoIfNeeded();
    } else if (update_background_status_cb_.IsCancelled()) {
      // Only trigger updates when we don't have one already scheduled.
      update_background_status_cb_.Reset(
          base::Bind(&WebMediaPlayerImpl::DisableVideoTrackIfNeeded,
                     base::Unretained(this)));

      // Defer disable track until we're sure the clip will be backgrounded for
      // some time. Resuming may take half a second, so frequent tab switches
      // will yield a poor user experience otherwise. http://crbug.com/709302
      // may also cause AV sync issues if disable/enable happens too fast.
      main_task_runner_->PostDelayedTask(
          FROM_HERE, update_background_status_cb_.callback(),
          base::TimeDelta::FromSeconds(10));
    }
  } else {
    update_background_status_cb_.Cancel();
    EnableVideoTrackIfNeeded();
  }
}

void WebMediaPlayerImpl::PauseVideoIfNeeded() {
  DCHECK(IsHidden());

  // Don't pause video while the pipeline is stopped, resuming or seeking.
  // Also if the video is paused already.
  if (!pipeline_controller_->IsPipelineRunning() || is_pipeline_resuming_ ||
      seeking_ || paused_)
    return;

  // OnPause() will set |paused_when_hidden_| to false and call
  // UpdatePlayState(), so set the flag to true after and then return.
  OnPause();
  paused_when_hidden_ = true;
}

void WebMediaPlayerImpl::EnableVideoTrackIfNeeded() {
  // Don't change video track while the pipeline is stopped, resuming or
  // seeking.
  if (!pipeline_controller_->IsPipelineRunning() || is_pipeline_resuming_ ||
      seeking_)
    return;

  if (video_track_disabled_) {
    video_track_disabled_ = false;
    if (client_->HasSelectedVideoTrack()) {
      WebMediaPlayer::TrackId trackId = client_->GetSelectedVideoTrackId();
      SelectedVideoTrackChanged(&trackId);
    }
  }
}

void WebMediaPlayerImpl::DisableVideoTrackIfNeeded() {
  DCHECK(IsHidden());

  // Don't change video track while the pipeline is resuming or seeking.
  if (is_pipeline_resuming_ || seeking_)
    return;

  if (!video_track_disabled_ && ShouldDisableVideoWhenHidden()) {
    video_track_disabled_ = true;
    SelectedVideoTrackChanged(nullptr);
  }
}

void WebMediaPlayerImpl::SetPipelineStatisticsForTest(
    const PipelineStatistics& stats) {
  pipeline_statistics_for_test_ = base::make_optional(stats);
}

PipelineStatistics WebMediaPlayerImpl::GetPipelineStatistics() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_statistics_for_test_.value_or(
      pipeline_controller_->GetStatistics());
}

void WebMediaPlayerImpl::SetPipelineMediaDurationForTest(
    base::TimeDelta duration) {
  pipeline_media_duration_for_test_ = base::make_optional(duration);
}

base::TimeDelta WebMediaPlayerImpl::GetPipelineMediaDuration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_media_duration_for_test_.value_or(
      pipeline_controller_->GetMediaDuration());
}

void WebMediaPlayerImpl::SwitchToRemoteRenderer(
    const std::string& remote_device_friendly_name) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  DCHECK(!is_remote_rendering_);
  is_remote_rendering_ = true;

  DCHECK(!disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = true;

  // Capabilities reporting should only be performed for local playbacks.
  video_decode_stats_reporter_.reset();

  // Requests to restart media pipeline. A remote renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
  if (client_) {
    client_->MediaRemotingStarted(
        WebString::FromUTF8(remote_device_friendly_name));
  }
}

void WebMediaPlayerImpl::SwitchToLocalRenderer(
    MediaObserverClient::ReasonToSwitchToLocal reason) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!is_remote_rendering_)
    return;  // Is currently with local renderer.
  is_remote_rendering_ = false;

  DCHECK(disable_pipeline_auto_suspend_);
  disable_pipeline_auto_suspend_ = false;

  // Capabilities reporting may resume now that playback is local.
  CreateVideoDecodeStatsReporter();

  // Requests to restart media pipeline. A local renderer will be created via
  // the |renderer_factory_selector_|.
  ScheduleRestart();
  if (client_)
    client_->MediaRemotingStopped(GetSwitchToLocalMessage(reason));
}

void WebMediaPlayerImpl::RecordUnderflowDuration(base::TimeDelta duration) {
  DCHECK(data_source_ || chunk_demuxer_);

  if (data_source_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.SRC", duration);
  else
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.MSE", duration);

  if (is_encrypted_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration2.EME", duration);
}

#define UMA_HISTOGRAM_VIDEO_HEIGHT(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 100, 10000, 50)

void WebMediaPlayerImpl::RecordVideoNaturalSize(const gfx::Size& natural_size) {
  // Always report video natural size to MediaLog.
  media_log_->AddEvent(media_log_->CreateVideoSizeSetEvent(
      natural_size.width(), natural_size.height()));

  if (initial_video_height_recorded_)
    return;

  initial_video_height_recorded_ = true;

  int height = natural_size.height();

  if (load_type_ == kLoadTypeURL)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.SRC", height);
  else if (load_type_ == kLoadTypeMediaSource)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.MSE", height);

  if (is_encrypted_)
    UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.EME", height);

  UMA_HISTOGRAM_VIDEO_HEIGHT("Media.VideoHeight.Initial.All", height);
}

#undef UMA_HISTOGRAM_VIDEO_HEIGHT

void WebMediaPlayerImpl::SetTickClockForTest(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  buffered_data_source_host_->SetTickClockForTest(tick_clock);
}

void WebMediaPlayerImpl::OnFirstFrame(base::TimeTicks frame_time) {
  DCHECK(!load_start_time_.is_null());
  DCHECK(!skip_metrics_due_to_startup_suspend_);
  has_first_frame_ = true;
  needs_first_frame_ = false;
  const base::TimeDelta elapsed = frame_time - load_start_time_;
  media_metrics_provider_->SetTimeToFirstFrame(elapsed);
  RecordTimingUMA("Media.TimeToFirstFrame", elapsed);

  // Needed to signal HTMLVideoElement that it should remove the poster image.
  if (client_ && has_poster_)
    client_->Repaint();
}

void WebMediaPlayerImpl::RecordTimingUMA(const std::string& key,
                                         base::TimeDelta elapsed) {
  if (chunk_demuxer_)
    base::UmaHistogramMediumTimes(key + ".MSE", elapsed);
  else
    base::UmaHistogramMediumTimes(key + ".SRC", elapsed);
  if (is_encrypted_)
    base::UmaHistogramMediumTimes(key + ".EME", elapsed);
}

void WebMediaPlayerImpl::RecordEncryptionScheme(
    const std::string& stream_name,
    const EncryptionScheme& encryption_scheme) {
  DCHECK(stream_name == "Audio" || stream_name == "Video");

  // If the stream is not encrypted, don't record it.
  if (encryption_scheme.mode() == EncryptionScheme::CIPHER_MODE_UNENCRYPTED)
    return;

  base::UmaHistogramEnumeration(
      "Media.EME.EncryptionScheme.Initial." + stream_name,
      DetermineEncryptionSchemeUMAValue(encryption_scheme),
      EncryptionSchemeUMA::kCount);
}

bool WebMediaPlayerImpl::IsInPictureInPicture() const {
  DCHECK(client_);
  return client_->DisplayType() ==
         WebMediaPlayer::DisplayType::kPictureInPicture;
}

void WebMediaPlayerImpl::MaybeSetContainerName() {
  // MSE nor MediaPlayerRenderer provide container information.
  if (chunk_demuxer_ || using_media_player_renderer_)
    return;

  // Pipeline startup failed before even getting a demuxer setup.
  if (!demuxer_)
    return;

  // Container has already been set.
  if (highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveMetadata)
    return;

// If ffmpeg isn't enabled, we can't get the container name.
#if BUILDFLAG(ENABLE_FFMPEG)
  media_metrics_provider_->SetContainerName(
      static_cast<FFmpegDemuxer*>(demuxer_.get())->container());
#endif
}

void WebMediaPlayerImpl::MaybeUpdateBufferSizesForPlayback() {
  // Don't increase the MultiBufferDataSource buffer size until we've reached
  // kReadyStateHaveEnoughData. Otherwise we will unnecessarily slow down
  // playback startup -- it can instead be done for free after playback starts.
  if (!mb_data_source_ || highest_ready_state_ < kReadyStateHaveEnoughData)
    return;

  mb_data_source_->MediaPlaybackRateChanged(playback_rate_);
  if (!paused_)
    mb_data_source_->MediaIsPlaying();
}

}  // namespace media
