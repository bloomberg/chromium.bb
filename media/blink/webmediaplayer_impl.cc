// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/base/limits.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_url_demuxer.h"
#include "media/base/text_renderer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/blink/texttrack_impl.h"
#include "media/blink/watch_time_reporter.h"
#include "media/blink/webaudiosourceprovider_impl.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webinbandtexttrack_impl.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/blink/webmediasource_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaTypes.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerEncryptedMediaClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

using blink::WebCanvas;
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

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const double kMinRate = 0.0625;
const double kMaxRate = 16.0;

void SetSinkIdOnMediaThread(scoped_refptr<WebAudioSourceProviderImpl> sink,
                            const std::string& device_id,
                            const url::Origin& security_origin,
                            const OutputDeviceStatusCB& callback) {
  sink->SwitchOutputDevice(device_id, security_origin, callback);
}

bool IsBackgroundedSuspendEnabled() {
#if !defined(OS_ANDROID)
  // Suspend/Resume is only enabled by default on Android.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaSuspend);
#else
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableMediaSuspend);
#endif
}

bool IsResumeBackgroundVideosEnabled() {
  return base::FeatureList::IsEnabled(kResumeBackgroundVideo);
}

bool IsBackgroundVideoTrackOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundVideoTrackOptimization);
}

bool IsBackgroundVideoPauseOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundVideoPauseOptimization);
}

#if defined(OS_ANDROID)

bool IsNewRemotePlaybackPipelineEnabled() {
  return base::FeatureList::IsEnabled(kNewRemotePlaybackPipeline);
}

#endif

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

base::TimeDelta GetCurrentTimeInternal(WebMediaPlayerImpl* p_this) {
  // We wrap currentTime() instead of using pipeline_controller_.GetMediaTime()
  // since there are a variety of cases in which that time is not accurate;
  // e.g., while remoting and during a pause or seek.
  return base::TimeDelta::FromSecondsD(p_this->CurrentTime());
}

// How much time must have elapsed since loading last progressed before we
// assume that the decoder will have had time to complete preroll.
constexpr base::TimeDelta kPrerollAttemptTimeout =
    base::TimeDelta::FromSeconds(3);

}  // namespace

class BufferedDataSourceHostImpl;

STATIC_ASSERT_ENUM(WebMediaPlayer::kCORSModeUnspecified,
                   UrlData::CORS_UNSPECIFIED);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCORSModeAnonymous, UrlData::CORS_ANONYMOUS);
STATIC_ASSERT_ENUM(WebMediaPlayer::kCORSModeUseCredentials,
                   UrlData::CORS_USE_CREDENTIALS);

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<RendererFactorySelector> renderer_factory_selector,
    linked_ptr<UrlIndex> url_index,
    std::unique_ptr<WebMediaPlayerParams> params)
    : frame_(frame),
      delegate_state_(DelegateState::GONE),
      delegate_has_audio_(false),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      highest_ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      preload_(MultibufferDataSource::AUTO),
      main_task_runner_(frame->LoadingTaskRunner()),
      media_task_runner_(params->media_task_runner()),
      worker_task_runner_(params->worker_task_runner()),
      media_log_(params->take_media_log()),
      pipeline_controller_(
          base::MakeUnique<PipelineImpl>(media_task_runner_, media_log_.get()),
          base::Bind(&WebMediaPlayerImpl::CreateRenderer,
                     base::Unretained(this)),
          base::Bind(&WebMediaPlayerImpl::OnPipelineSeeked, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnPipelineSuspended, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnBeforePipelineResume, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnPipelineResumed, AsWeakPtr()),
          base::Bind(&WebMediaPlayerImpl::OnError, AsWeakPtr())),
      load_type_(kLoadTypeURL),
      opaque_(false),
      playback_rate_(0.0),
      paused_(true),
      paused_when_hidden_(false),
      seeking_(false),
      pending_suspend_resume_cycle_(false),
      ended_(false),
      should_notify_time_changed_(false),
      overlay_enabled_(false),
      decoder_requires_restart_for_overlay_(false),
      client_(client),
      encrypted_client_(encrypted_client),
      delegate_(delegate),
      delegate_id_(0),
      defer_load_cb_(params->defer_load_cb()),
      context_3d_cb_(params->context_3d_cb()),
      adjust_allocated_memory_cb_(params->adjust_allocated_memory_cb()),
      last_reported_memory_usage_(0),
      supports_save_(true),
      chunk_demuxer_(NULL),
      tick_clock_(new base::DefaultTickClock()),
      buffered_data_source_host_(
          base::Bind(&WebMediaPlayerImpl::OnProgress, AsWeakPtr()),
          tick_clock_.get()),
      url_index_(url_index),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(params->compositor_task_runner()
                                  ? params->compositor_task_runner()
                                  : base::ThreadTaskRunnerHandle::Get()),
      compositor_(new VideoFrameCompositor(compositor_task_runner_)),
#if defined(OS_ANDROID)  // WMPI_CAST
      cast_impl_(this, client_, params->context_3d_cb()),
#endif
      volume_(1.0),
      volume_multiplier_(1.0),
      renderer_factory_selector_(std::move(renderer_factory_selector)),
      surface_manager_(params->surface_manager()),
      overlay_surface_id_(SurfaceManager::kNoSurfaceID),
      suppress_destruction_errors_(false),
      suspend_enabled_(params->allow_suspend()),
      is_encrypted_(false),
      preroll_attempt_pending_(false),
      observer_(params->media_observer()),
      max_keyframe_distance_to_disable_background_video_(
          params->max_keyframe_distance_to_disable_background_video()),
      max_keyframe_distance_to_disable_background_video_mse_(
          params->max_keyframe_distance_to_disable_background_video_mse()),
      enable_instant_source_buffer_gc_(
          params->enable_instant_source_buffer_gc()),
      embedded_media_experience_enabled_(
          params->embedded_media_experience_enabled()),
      request_routing_token_cb_(params->request_routing_token_cb()),
      overlay_routing_token_(OverlayInfo::RoutingToken()) {
  DVLOG(1) << __func__;
  DCHECK(!adjust_allocated_memory_cb_.is_null());
  DCHECK(renderer_factory_selector_);
  DCHECK(client_);
  DCHECK(delegate_);

  force_video_overlays_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceVideoOverlays);

  if (base::FeatureList::IsEnabled(media::kOverlayFullscreenVideo)) {
    bool use_android_overlay =
        base::FeatureList::IsEnabled(media::kUseAndroidOverlay);
    overlay_mode_ = use_android_overlay ? OverlayMode::kUseAndroidOverlay
                                        : OverlayMode::kUseContentVideoView;
  } else {
    overlay_mode_ = OverlayMode::kNoOverlays;
  }

  delegate_id_ = delegate_->AddObserver(this);
  delegate_->SetIdle(delegate_id_, true);

  media_log_->AddEvent(media_log_->CreateCreatedEvent(
      url::Origin(frame_->GetSecurityOrigin()).GetURL().spec()));

  if (params->initial_cdm())
    SetCdm(params->initial_cdm());

  // TODO(xhwang): When we use an external Renderer, many methods won't work,
  // e.g. GetCurrentFrameFromCompositor(). See http://crbug.com/434861
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      params->audio_renderer_sink(), media_log_.get());

  if (observer_)
    observer_->SetClient(this);
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (set_cdm_result_) {
    DVLOG(2) << "Resolve pending SetCdm() when media player is destroyed.";
    set_cdm_result_->Complete();
    set_cdm_result_.reset();
  }

  suppress_destruction_errors_ = true;

  delegate_->PlayerGone(delegate_id_);
  delegate_->RemoveObserver(delegate_id_);

  // Finalize any watch time metrics before destroying the pipeline.
  watch_time_reporter_.reset();

  // The underlying Pipeline must be stopped before it is destroyed.
  pipeline_controller_.Stop();

  if (last_reported_memory_usage_)
    adjust_allocated_memory_cb_.Run(-last_reported_memory_usage_);

  // Destruct compositor resources in the proper order.
  client_->SetWebLayer(nullptr);
  if (video_weblayer_)
    static_cast<cc::VideoLayer*>(video_weblayer_->layer())->StopUsingProvider();
  compositor_task_runner_->DeleteSoon(FROM_HERE, compositor_);

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));
}

void WebMediaPlayerImpl::Load(LoadType load_type,
                              const blink::WebMediaPlayerSource& source,
                              CORSMode cors_mode) {
  DVLOG(1) << __func__;
  // Only URL or MSE blob URL is supported.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();
  DVLOG(1) << __func__ << "(" << load_type << ", " << url << ", " << cors_mode
           << ")";
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(&WebMediaPlayerImpl::DoLoad, AsWeakPtr(),
                                  load_type, url, cors_mode));
    return;
  }
  DoLoad(load_type, url, cors_mode);
}

bool WebMediaPlayerImpl::SupportsOverlayFullscreenVideo() {
#if defined(OS_ANDROID)
  return !using_media_player_renderer_;
#else
  return false;
#endif
}

void WebMediaPlayerImpl::EnableOverlay() {
  overlay_enabled_ = true;
  if (surface_manager_ && overlay_mode_ == OverlayMode::kUseContentVideoView) {
    overlay_surface_id_.reset();
    surface_created_cb_.Reset(
        base::Bind(&WebMediaPlayerImpl::OnSurfaceCreated, AsWeakPtr()));
    surface_manager_->CreateFullscreenSurface(pipeline_metadata_.natural_size,
                                              surface_created_cb_.callback());
  } else if (request_routing_token_cb_ &&
             overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    overlay_routing_token_is_pending_ = true;
    token_available_cb_.Reset(
        base::Bind(&WebMediaPlayerImpl::OnOverlayRoutingToken, AsWeakPtr()));
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
    overlay_surface_id_ = SurfaceManager::kNoSurfaceID;
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

  // |force_video_overlays_| implies that we're already in overlay mode, so take
  // no action here.  Otherwise, switch to an overlay if it's allowed and if
  // it will display properly.
  if (!force_video_overlays_ && overlay_mode_ != OverlayMode::kNoOverlays &&
      DoesOverlaySupportMetadata()) {
    EnableOverlay();
  }
  if (observer_)
    observer_->OnEnteredFullscreen();

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

  // If we're in overlay mode, then exit it unless we're supposed to be in
  // overlay mode all the time.
  if (!force_video_overlays_ && overlay_enabled_)
    DisableOverlay();
  if (observer_)
    observer_->OnExitedFullscreen();

  // See EnteredFullscreen for why we do this.
  if (!decoder_requires_restart_for_overlay_)
    MaybeSendOverlayInfoToDecoder();
}

void WebMediaPlayerImpl::BecameDominantVisibleContent(bool isDominant) {
  if (observer_)
    observer_->OnBecameDominantVisibleContent(isDominant);
}

void WebMediaPlayerImpl::SetIsEffectivelyFullscreen(
    bool isEffectivelyFullscreen) {
  delegate_->SetIsEffectivelyFullscreen(delegate_id_, isEffectivelyFullscreen);
}

void WebMediaPlayerImpl::OnHasNativeControlsChanged(bool has_native_controls) {
  if (!watch_time_reporter_)
    return;

  if (has_native_controls)
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CORSMode cors_mode) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  GURL gurl(url);
  ReportMetrics(load_type, gurl, frame_->GetSecurityOrigin(), media_log_.get());

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", gurl.spec());

  // Used for HLS playback.
  loaded_url_ = gurl;

  load_type_ = load_type;

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.GetString().Utf8()));

  // Media source pipelines can start immediately.
  if (load_type == kLoadTypeMediaSource) {
    supports_save_ = false;
    StartPipeline();
  } else {
    data_source_.reset(new MultibufferDataSource(
        url, static_cast<UrlData::CORSMode>(cors_mode), main_task_runner_,
        url_index_, frame_, media_log_.get(), &buffered_data_source_host_,
        base::Bind(&WebMediaPlayerImpl::NotifyDownloading, AsWeakPtr())));
    data_source_->SetPreload(preload_);
    data_source_->Initialize(
        base::Bind(&WebMediaPlayerImpl::DataSourceInitialized, AsWeakPtr()));
  }

#if defined(OS_ANDROID)  // WMPI_CAST
  cast_impl_.Initialize(url, frame_, delegate_id_);
#endif
}

void WebMediaPlayerImpl::Play() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // User initiated play unlocks background video playback.
  if (blink::WebUserGestureIndicator::IsProcessingUserGesture())
    video_locked_when_paused_when_hidden_ = false;

#if defined(OS_ANDROID)  // WMPI_CAST
  if (IsRemote()) {
    cast_impl_.play();
    return;
  }
#endif
  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  paused_ = false;
  pipeline_controller_.SetPlaybackRate(playback_rate_);
  background_pause_timer_.Stop();

  if (data_source_)
    data_source_->MediaIsPlaying();

  if (observer_)
    observer_->OnPlaying();

  DCHECK(watch_time_reporter_);
  watch_time_reporter_->OnPlaying();
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PLAY));
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
  if (blink::WebUserGestureIndicator::IsProcessingUserGesture())
    video_locked_when_paused_when_hidden_ = true;

#if defined(OS_ANDROID)  // WMPI_CAST
  if (IsRemote()) {
    cast_impl_.pause();
    return;
  }
#endif

  pipeline_controller_.SetPlaybackRate(0.0);

  // pause() may be called after playback has ended and the HTMLMediaElement
  // requires that currentTime() == duration() after ending.  We want to ensure
  // |paused_time_| matches currentTime() in this case or a future seek() may
  // incorrectly discard what it thinks is a seek to the existing time.
  paused_time_ =
      ended_ ? GetPipelineMediaDuration() : pipeline_controller_.GetMediaTime();

  if (observer_)
    observer_->OnPaused();

  DCHECK(watch_time_reporter_);
  watch_time_reporter_->OnPaused();
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PAUSE));

  UpdatePlayState();
}

bool WebMediaPlayerImpl::SupportsSave() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return supports_save_;
}

void WebMediaPlayerImpl::Seek(double seconds) {
  DVLOG(1) << __func__ << "(" << seconds << "s)";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));
  DoSeek(base::TimeDelta::FromSecondsD(seconds), true);
}

void WebMediaPlayerImpl::DoSeek(base::TimeDelta time, bool time_updated) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  if (IsRemote()) {
    cast_impl_.seek(time);
    return;
  }
#endif

  ReadyState old_state = ready_state_;
  if (ready_state_ > WebMediaPlayer::kReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);

  // When paused, we know exactly what the current time is and can elide seeks
  // to it. However, there are two cases that are not elided:
  //   1) When the pipeline state is not stable.
  //      In this case we just let |pipeline_controller_| decide what to do, as
  //      it has complete information.
  //   2) For MSE.
  //      Because the buffers may have changed between seeks, MSE seeks are
  //      never elided.
  if (paused_ && pipeline_controller_.IsStable() && paused_time_ == time &&
      !chunk_demuxer_) {
    // If the ready state was high enough before, we can indicate that the seek
    // completed just by restoring it. Otherwise we will just wait for the real
    // ready state change to eventually happen.
    if (old_state == kReadyStateHaveEnoughData) {
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&WebMediaPlayerImpl::OnBufferingStateChange,
                                AsWeakPtr(), BUFFERING_HAVE_ENOUGH));
    }
    return;
  }

  // Call this before setting |seeking_| so that the current media time can be
  // recorded by the reporter.
  if (watch_time_reporter_)
    watch_time_reporter_->OnSeeking();

  // Clear any new frame processed callbacks on seek; otherwise we'll end up
  // logging a time long after the seek completes.
  frame_time_report_cb_.Cancel();

  // TODO(sandersd): Move |seeking_| to PipelineController.
  // TODO(sandersd): Do we want to reset the idle timer here?
  delegate_->SetIdle(delegate_id_, false);
  ended_ = false;
  seeking_ = true;
  seek_time_ = time;
  if (paused_)
    paused_time_ = time;
  pipeline_controller_.Seek(time, time_updated);

  // This needs to be called after Seek() so that if a resume is triggered, it
  // is to the correct time.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetRate(double rate) {
  DVLOG(1) << __func__ << "(" << rate << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0)
    return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  playback_rate_ = rate;
  if (!paused_) {
    pipeline_controller_.SetPlaybackRate(rate);
    if (data_source_)
      data_source_->MediaPlaybackRateChanged(rate);
  }
}

void WebMediaPlayerImpl::SetVolume(double volume) {
  DVLOG(1) << __func__ << "(" << volume << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  volume_ = volume;
  pipeline_controller_.SetVolume(volume_ * volume_multiplier_);
  if (watch_time_reporter_)
    watch_time_reporter_->OnVolumeChange(volume);
  delegate_->DidPlayerMutedStatusChange(delegate_id_, volume == 0.0);

  // The play state is updated because the player might have left the autoplay
  // muted state.
  UpdatePlayState();
}

void WebMediaPlayerImpl::SetSinkId(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  media::OutputDeviceStatusCB callback =
      media::ConvertToOutputDeviceStatusCB(web_callback);
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SetSinkIdOnMediaThread, audio_source_provider_,
                 sink_id.Utf8(), static_cast<url::Origin>(security_origin),
                 callback));
}

STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadNone, MultibufferDataSource::NONE);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadMetaData,
                   MultibufferDataSource::METADATA);
STATIC_ASSERT_ENUM(WebMediaPlayer::kPreloadAuto, MultibufferDataSource::AUTO);

void WebMediaPlayerImpl::SetPreload(WebMediaPlayer::Preload preload) {
  DVLOG(1) << __func__ << "(" << preload << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  preload_ = static_cast<MultibufferDataSource::Preload>(preload);
  if (data_source_)
    data_source_->SetPreload(preload_);
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
    MediaTrack::Id track_id = blinkTrackId.Utf8().data();
    logstr << track_id << " ";
    enabledMediaTrackIds.push_back(track_id);
  }
  MEDIA_LOG(INFO, media_log_.get())
      << "Enabled audio tracks: [" << logstr.str() << "]";
  pipeline_controller_.OnEnabledAudioTracksChanged(enabledMediaTrackIds);
}

void WebMediaPlayerImpl::SelectedVideoTrackChanged(
    blink::WebMediaPlayer::TrackId* selectedTrackId) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  base::Optional<MediaTrack::Id> selected_video_track_id;
  if (selectedTrackId && !video_track_disabled_)
    selected_video_track_id = MediaTrack::Id(selectedTrackId->Utf8().data());
  MEDIA_LOG(INFO, media_log_.get())
      << "Selected video track: [" << selected_video_track_id.value_or("")
      << "]";
  pipeline_controller_.OnSelectedVideoTrackChanged(selected_video_track_id);
}

bool WebMediaPlayerImpl::GetLastUploadedFrameInfo(unsigned* width,
                                                  unsigned* height,
                                                  double* timestamp) {
  *width = last_uploaded_frame_size_.width();
  *height = last_uploaded_frame_size_.height();
  *timestamp = last_uploaded_frame_timestamp_.InSecondsF();
  return true;
}

blink::WebSize WebMediaPlayerImpl::NaturalSize() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return blink::WebSize(pipeline_metadata_.natural_size);
}

bool WebMediaPlayerImpl::Paused() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  if (IsRemote())
    return cast_impl_.IsPaused();
#endif

  return pipeline_controller_.GetPlaybackRate() == 0.0f;
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

double WebMediaPlayerImpl::CurrentTime() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  if (ended_)
    return Duration();

  if (Seeking())
    return seek_time_.InSecondsF();

#if defined(OS_ANDROID)  // WMPI_CAST
  if (IsRemote())
    return cast_impl_.currentTime();
#endif

  if (paused_)
    return paused_time_.InSecondsF();

  return pipeline_controller_.GetMediaTime().InSecondsF();
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::GetNetworkState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::GetReadyState() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return ready_state_;
}

blink::WebString WebMediaPlayerImpl::GetErrorMessage() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

blink::WebTimeRanges WebMediaPlayerImpl::Buffered() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Ranges<base::TimeDelta> buffered_time_ranges =
      pipeline_controller_.GetBufferedTimeRanges();

  const base::TimeDelta duration = GetPipelineMediaDuration();
  if (duration != kInfiniteDuration) {
    buffered_data_source_host_.AddBufferedTimeRanges(&buffered_time_ranges,
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
  const bool is_finite_stream = data_source_ && data_source_->IsStreaming() &&
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
  if (highest_ready_state_ >= ReadyState::kReadyStateHaveFutureData)
    return false;

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
  const bool pipeline_progress = pipeline_controller_.DidLoadingProgress();
  const bool data_progress = buffered_data_source_host_.DidLoadingProgress();
  return pipeline_progress || data_progress;
}

void WebMediaPlayerImpl::Paint(blink::WebCanvas* canvas,
                               const blink::WebRect& rect,
                               cc::PaintFlags& flags) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");

  // We can't copy from protected frames.
  if (cdm_)
    return;

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();

  gfx::Rect gfx_rect(rect);
  Context3D context_3d;
  if (video_frame.get() && video_frame->HasTextures()) {
    if (!context_3d_cb_.is_null())
      context_3d = context_3d_cb_.Run();
    if (!context_3d.gl)
      return;  // Unable to get/create a shared main thread context.
    if (!context_3d.gr_context)
      return;  // The context has been lost since and can't setup a GrContext.
  }
  skcanvas_video_renderer_.Paint(video_frame, canvas, gfx::RectF(gfx_rect),
                                 flags, pipeline_metadata_.video_rotation,
                                 context_3d);
}

bool WebMediaPlayerImpl::HasSingleSecurityOrigin() const {
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::DidPassCORSAccessCheck() const {
  if (data_source_)
    return data_source_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerImpl::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::DecodedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::DroppedFrameCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_frames_dropped;
}

size_t WebMediaPlayerImpl::AudioDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = GetPipelineStatistics();
  return stats.audio_bytes_decoded;
}

size_t WebMediaPlayerImpl::VideoDecodedByteCount() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_bytes_decoded;
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
    bool flip_y) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl:copyVideoTextureToPlatformTexture");

  // We can't copy from protected frames.
  if (cdm_)
    return false;

  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame.get() || !video_frame->HasTextures()) {
    return false;
  }

  Context3D context_3d;
  if (!context_3d_cb_.is_null())
    context_3d = context_3d_cb_.Run();
  return skcanvas_video_renderer_.CopyVideoFrameTexturesToGLTexture(
      context_3d, gl, video_frame.get(), target, texture, internal_format,
      format, type, level, premultiply_alpha, flip_y);
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

  // Recreate the watch time reporter if necessary.
  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;
  if (!was_encrypted && watch_time_reporter_)
    CreateWatchTimeReporter();

  SetCdm(cdm);
}

void WebMediaPlayerImpl::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(init_data_type != EmeInitDataType::UNKNOWN);

  // TODO(xhwang): Update this UMA name. https://crbug.com/589251
  UMA_HISTOGRAM_COUNTS("Media.EME.NeedKey", 1);

  // Recreate the watch time reporter if necessary.
  const bool was_encrypted = is_encrypted_;
  is_encrypted_ = true;
  if (!was_encrypted && watch_time_reporter_)
    CreateWatchTimeReporter();

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
      client_->AddAudioTrack(blink::WebString::FromUTF8(track->id()),
                             blink::WebMediaPlayerClient::kAudioTrackKindMain,
                             blink::WebString::FromUTF8(track->label()),
                             blink::WebString::FromUTF8(track->language()),
                             is_first_audio_track);
      is_first_audio_track = false;
    } else if (track->type() == MediaTrack::Video) {
      client_->AddVideoTrack(blink::WebString::FromUTF8(track->id()),
                             blink::WebMediaPlayerClient::kVideoTrackKindMain,
                             blink::WebString::FromUTF8(track->label()),
                             blink::WebString::FromUTF8(track->language()),
                             is_first_video_track);
      is_first_video_track = false;
    } else {
      // Text tracks are not supported through this code path yet.
      NOTREACHED();
    }
  }
}

void WebMediaPlayerImpl::SetCdm(blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(cdm);
  scoped_refptr<ContentDecryptionModule> cdm_reference =
      ToWebContentDecryptionModuleImpl(cdm)->GetCdm();
  if (!cdm_reference) {
    NOTREACHED();
    OnCdmAttached(false);
    return;
  }

  CdmContext* cdm_context = cdm_reference->GetCdmContext();
  if (!cdm_context) {
    OnCdmAttached(false);
    return;
  }

  if (observer_)
    observer_->OnSetCdm(cdm_context);

  // Keep the reference to the CDM, as it shouldn't be destroyed until
  // after the pipeline is done with the |cdm_context|.
  pending_cdm_ = std::move(cdm_reference);
  pipeline_controller_.SetCdm(
      cdm_context, base::Bind(&WebMediaPlayerImpl::OnCdmAttached, AsWeakPtr()));
}

void WebMediaPlayerImpl::OnCdmAttached(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_cdm_);

  // If the CDM is set from the constructor there is no promise
  // (|set_cdm_result_|) to fulfill.
  if (success) {
    media_log_->SetBooleanProperty("has_cdm", true);

    // This will release the previously attached CDM (if any).
    cdm_ = std::move(pending_cdm_);
    if (set_cdm_result_) {
      set_cdm_result_->Complete();
      set_cdm_result_.reset();
    }

    return;
  }

  pending_cdm_ = nullptr;
  if (set_cdm_result_) {
    set_cdm_result_->CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        "Unable to set ContentDecryptionModule object");
    set_cdm_result_.reset();
  }
}

void WebMediaPlayerImpl::OnPipelineSeeked(bool time_updated) {
  seeking_ = false;
  seek_time_ = base::TimeDelta();

  if (paused_) {
#if defined(OS_ANDROID)  // WMPI_CAST
    if (IsRemote()) {
      paused_time_ = base::TimeDelta::FromSecondsD(cast_impl_.currentTime());
    } else {
      paused_time_ = pipeline_controller_.GetMediaTime();
    }
#else
    paused_time_ = pipeline_controller_.GetMediaTime();
#endif
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
}

void WebMediaPlayerImpl::OnPipelineSuspended() {
#if defined(OS_ANDROID)
  if (IsRemote() && !IsNewRemotePlaybackPipelineEnabled()) {
    scoped_refptr<VideoFrame> frame = cast_impl_.GetCastingBanner();
    if (frame)
      compositor_->PaintSingleFrame(frame);
  }
#endif

  // Tell the data source we have enough data so that it may release the
  // connection.
  if (data_source_)
    data_source_->OnBufferingHaveEnough(true);

  ReportMemoryUsage();

  if (pending_suspend_resume_cycle_) {
    pending_suspend_resume_cycle_ = false;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::OnBeforePipelineResume() {
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

  // base::Unretained is safe, since chunk_demuxer_ is actually owned by
  // |this| via this->demuxer_.
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChunkDemuxer::OnMemoryPressure,
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

  ReportPipelineError(load_type_, status, media_log_.get());
  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(status));

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
  } else {
    SetNetworkState(PipelineErrorToNetworkState(status));
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnEnded() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore state changes until we've completed all outstanding operations.
  if (!pipeline_controller_.IsStable())
    return;

  ended_ = true;
  client_->TimeChanged();

  // Clear any new frame processed callbacks on end; otherwise we'll end up
  // logging a time long after playback ends.
  frame_time_report_cb_.Cancel();

  // We don't actually want this to run until |client_| calls seek() or pause(),
  // but that should have already happened in timeChanged() and so this is
  // expected to be a no-op.
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnMetadata(PipelineMetadata metadata) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  pipeline_metadata_ = metadata;

  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
  UMA_HISTOGRAM_ENUMERATION("Media.VideoRotation", metadata.video_rotation,
                            VIDEO_ROTATION_MAX + 1);

  if (HasVideo()) {
    if (overlay_enabled_) {
      // SurfaceView doesn't support rotated video, so transition back if
      // the video is now rotated.  If |force_video_overlays_|, we keep the
      // overlay anyway so that the state machine keeps working.
      if (!force_video_overlays_ && !DoesOverlaySupportMetadata())
        DisableOverlay();
      else if (surface_manager_)
        surface_manager_->NaturalSizeChanged(pipeline_metadata_.natural_size);
    }

    DCHECK(!video_weblayer_);
    video_weblayer_.reset(new cc_blink::WebLayerImpl(cc::VideoLayer::Create(
        compositor_, pipeline_metadata_.video_rotation)));
    video_weblayer_->layer()->SetContentsOpaque(opaque_);
    video_weblayer_->SetContentsOpaqueIsFixed(true);
    client_->SetWebLayer(video_weblayer_.get());
  }

  if (observer_)
    observer_->OnMetadataChanged(pipeline_metadata_);

  CreateWatchTimeReporter();
  UpdatePlayState();
}

void WebMediaPlayerImpl::OnProgress() {
  DVLOG(4) << __func__;
  if (highest_ready_state_ < ReadyState::kReadyStateHaveFutureData) {
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
  if (data_source_ && data_source_->assume_fully_buffered())
    return true;
  // If we're not currently downloading, we have as much buffer as
  // we're ever going to get, which means we say we can play through.
  if (network_state_ == WebMediaPlayer::kNetworkStateIdle)
    return true;
  return buffered_data_source_host_.CanPlayThrough(
      base::TimeDelta::FromSecondsD(CurrentTime()),
      base::TimeDelta::FromSecondsD(Duration()),
      playback_rate_ == 0.0 ? 1.0 : playback_rate_);
}

void WebMediaPlayerImpl::OnBufferingStateChange(BufferingState state) {
  DVLOG(1) << __func__ << "(" << state << ")";
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Ignore buffering state changes until we've completed all outstanding
  // operations.
  if (!pipeline_controller_.IsStable())
    return;

  media_log_->AddEvent(media_log_->CreateBufferingStateChangedEvent(
      "pipeline_buffering_state", state));

  if (state == BUFFERING_HAVE_ENOUGH) {
    if (highest_ready_state_ < WebMediaPlayer::kReadyStateHaveEnoughData) {
      // Record a zero value for underflow histogram so that the histogram
      // includes playbacks which never encounter an underflow event.
      RecordUnderflowDuration(base::TimeDelta());
    }

    SetReadyState(CanPlayThrough() ? WebMediaPlayer::kReadyStateHaveEnoughData
                                   : WebMediaPlayer::kReadyStateHaveFutureData);

    // Let the DataSource know we have enough data. It may use this information
    // to release unused network connections.
    if (data_source_)
      data_source_->OnBufferingHaveEnough(false);

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

  // TODO(sandersd): We should call delegate_->DidPlay() with the new duration,
  // especially if it changed from  <5s to >5s.
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return;

  client_->DurationChanged();
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

  std::unique_ptr<media::TextTrack> text_track(new TextTrackImpl(
      main_task_runner_, client_, std::move(web_inband_text_track)));

  done_cb.Run(std::move(text_track));
}

void WebMediaPlayerImpl::OnWaitingForDecryptionKey() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  encrypted_client_->DidBlockPlaybackWaitingForKey();
  // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
  // when a key has been successfully added (e.g. OnSessionKeysChange() with
  // |has_additional_usable_key| = true). http://crbug.com/461903
  encrypted_client_->DidResumePlaybackBlockedForKey();
}

void WebMediaPlayerImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::kReadyStateHaveNothing);

  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnNaturalSizeChanged");

  // The input |size| is from the decoded video frame, which is the original
  // natural size and need to be rotated accordingly.
  gfx::Size rotated_size =
      GetRotatedVideoSize(pipeline_metadata_.video_rotation, size);

  RecordVideoNaturalSize(rotated_size);

  gfx::Size old_size = pipeline_metadata_.natural_size;
  if (rotated_size == old_size)
    return;

  pipeline_metadata_.natural_size = rotated_size;

  // Re-create |watch_time_reporter_| if we didn't originally know the video
  // size or the previous size was too small for reporting.
  if (!watch_time_reporter_->IsSizeLargeEnoughToReportWatchTime())
    CreateWatchTimeReporter();

  if (overlay_enabled_ && surface_manager_ &&
      overlay_mode_ == OverlayMode::kUseContentVideoView) {
    surface_manager_->NaturalSizeChanged(rotated_size);
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
  // Modify content opaqueness of cc::Layer directly so that
  // SetContentsOpaqueIsFixed is ignored.
  if (video_weblayer_)
    video_weblayer_->layer()->SetContentsOpaque(opaque_);
}

void WebMediaPlayerImpl::OnVideoAverageKeyframeDistanceUpdate() {
  UpdateBackgroundVideoOptimizationState();
}

void WebMediaPlayerImpl::OnFrameHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Backgrounding a video requires a user gesture to resume playback.
  if (IsHidden())
    video_locked_when_paused_when_hidden_ = true;

  if (watch_time_reporter_)
    watch_time_reporter_->OnHidden();

  UpdateBackgroundVideoOptimizationState();
  UpdatePlayState();

  // Schedule suspended playing media to be paused if the user doesn't come back
  // to it within some timeout period to avoid any autoplay surprises.
  ScheduleIdlePauseTimer();
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

  // Only track the time to the first frame if playing or about to play because
  // of being shown and only for videos we would optimize background playback
  // for.
  if ((!paused_ && IsBackgroundOptimizationCandidate()) ||
      paused_when_hidden_) {
    frame_time_report_cb_.Reset(
        base::Bind(&WebMediaPlayerImpl::ReportTimeFromForegroundToFirstFrame,
                   AsWeakPtr(), base::TimeTicks::Now()));
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoFrameCompositor::SetOnNewProcessedFrameCallback,
                   base::Unretained(compositor_),
                   BindToCurrentLoop(frame_time_report_cb_.callback())));
  }

  EnableVideoTrackIfNeeded();

  if (paused_when_hidden_) {
    paused_when_hidden_ = false;
    OnPlay();  // Calls UpdatePlayState() so return afterwards.
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnIdleTimeout() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // If we are attempting preroll, clear the stale flag.
  if (IsPrerollAttemptNeeded()) {
    delegate_->ClearStaleFlag(delegate_id_);
    return;
  }

  UpdatePlayState();
}

void WebMediaPlayerImpl::OnPlay() {
  Play();
  client_->PlaybackStateChanged();
}

void WebMediaPlayerImpl::OnPause() {
  Pause();
  client_->PlaybackStateChanged();
}

void WebMediaPlayerImpl::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  SetVolume(volume_);
}

void WebMediaPlayerImpl::OnBecamePersistentVideo(bool value) {
  client_->OnBecamePersistentVideo(value);
}

void WebMediaPlayerImpl::ScheduleRestart() {
  // TODO(watk): All restart logic should be moved into PipelineController.
  if (pipeline_controller_.IsPipelineRunning() &&
      !pipeline_controller_.IsPipelineSuspended()) {
    pending_suspend_resume_cycle_ = true;
    UpdatePlayState();
  }
}

void WebMediaPlayerImpl::RequestRemotePlaybackDisabled(bool disabled) {
  if (observer_)
    observer_->OnRemotePlaybackDisabled(disabled);
}

#if defined(OS_ANDROID)  // WMPI_CAST
bool WebMediaPlayerImpl::IsRemote() const {
  return cast_impl_.isRemote();
}

void WebMediaPlayerImpl::SetMediaPlayerManager(
    RendererMediaPlayerManagerInterface* media_player_manager) {
  cast_impl_.SetMediaPlayerManager(media_player_manager);
}

void WebMediaPlayerImpl::RequestRemotePlayback() {
  cast_impl_.requestRemotePlayback();
}

void WebMediaPlayerImpl::RequestRemotePlaybackControl() {
  cast_impl_.requestRemotePlaybackControl();
}

void WebMediaPlayerImpl::RequestRemotePlaybackStop() {
  cast_impl_.requestRemotePlaybackStop();
}

void WebMediaPlayerImpl::OnRemotePlaybackEnded() {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  ended_ = true;
  client_->TimeChanged();
}

void WebMediaPlayerImpl::OnDisconnectedFromRemoteDevice(double t) {
  DoSeek(base::TimeDelta::FromSecondsD(t), false);

  // We already told the delegate we're paused when remoting started.
  client_->PlaybackStateChanged();
  client_->DisconnectedFromRemoteDevice();

  UpdatePlayState();
}

void WebMediaPlayerImpl::SuspendForRemote() {
  if (pipeline_controller_.IsPipelineSuspended() &&
      !IsNewRemotePlaybackPipelineEnabled()) {
    scoped_refptr<VideoFrame> frame = cast_impl_.GetCastingBanner();
    if (frame)
      compositor_->PaintSingleFrame(frame);
  }

  UpdatePlayState();
}

gfx::Size WebMediaPlayerImpl::GetCanvasSize() const {
  if (!video_weblayer_)
    return pipeline_metadata_.natural_size;

  return video_weblayer_->Bounds();
}

void WebMediaPlayerImpl::SetDeviceScaleFactor(float scale_factor) {
  cast_impl_.SetDeviceScaleFactor(scale_factor);
}

void WebMediaPlayerImpl::SetPoster(const blink::WebURL& poster) {
  cast_impl_.setPoster(poster);
}
#endif  // defined(OS_ANDROID)  // WMPI_CAST

void WebMediaPlayerImpl::DataSourceInitialized(bool success) {
  DVLOG(1) << __func__;
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)
  // We can't play HLS URLs with WebMediaPlayerImpl, so in cases where they are
  // encountered, instruct the HTML media element to use the MediaPlayerRenderer
  // instead.
  //
  // TODO(tguilbert): Detect the presence of HLS based on demuxing results,
  // rather than the URL string. See crbug.com/663503.
  if (data_source_) {
    const GURL url_after_redirects = data_source_->GetUrlAfterRedirects();
    if (MediaCodecUtil::IsHLSURL(url_after_redirects)) {
      renderer_factory_selector_->SetUseMediaPlayer(true);
    }
  }
#endif

  if (!success) {
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);

    // Not really necessary, since the pipeline was never started, but it at
    // least this makes sure that the error handling code is in sync.
    UpdatePlayState();

    return;
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

void WebMediaPlayerImpl::OnSurfaceCreated(int surface_id) {
  DCHECK(overlay_mode_ == OverlayMode::kUseContentVideoView);
  overlay_surface_id_ = surface_id;
  MaybeSendOverlayInfoToDecoder();
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
  DCHECK(surface_manager_);

  // A null callback indicates that the decoder is going away.
  if (provide_overlay_info_cb.is_null()) {
    decoder_requires_restart_for_overlay_ = false;
    provide_overlay_info_cb_.Reset();
    return;
  }

  // If we get a surface request it means GpuVideoDecoder is initializing, so
  // until we get a null surface request, GVD is the active decoder.
  //
  // If |decoder_requires_restart_for_overlay| is true, we must restart the
  // pipeline for fullscreen transitions. The decoder is unable to switch
  // surfaces otherwise. If false, we simply need to tell the decoder about the
  // new surface and it will handle things seamlessly.
  decoder_requires_restart_for_overlay_ = decoder_requires_restart_for_overlay;
  provide_overlay_info_cb_ = provide_overlay_info_cb;

  // If we're waiting for the surface to arrive, OnSurfaceCreated() will be
  // called later when it arrives; so do nothing for now.  For AndroidOverlay,
  // if we're waiting for the token then... OnOverlayRoutingToken()...
  // We do this so that a request for a surface will block if we're in the
  // process of getting one.  Otherwise, on pre-M, the decoder would be stuck
  // without an overlay if the restart that happens on entering fullscreen
  // succeeds before we have the overlay info.  Post-M, we could send what we
  // have unconditionally.  When the info arrives, it will be sent.
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
  if (overlay_mode_ == OverlayMode::kUseContentVideoView) {
    if (!overlay_surface_id_.has_value())
      return;

    overlay_info_.surface_id = *overlay_surface_id_;
  } else if (overlay_mode_ == OverlayMode::kUseAndroidOverlay) {
    if (overlay_routing_token_is_pending_)
      return;

    overlay_info_.routing_token = overlay_routing_token_;
  }

  // If restart is required, the callback is one-shot only.
  if (decoder_requires_restart_for_overlay_) {
    base::ResetAndReturn(&provide_overlay_info_cb_).Run(overlay_info_);
  } else {
    provide_overlay_info_cb_.Run(overlay_info_);
  }
}

std::unique_ptr<Renderer> WebMediaPlayerImpl::CreateRenderer() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // TODO(liberato): Re-evaluate this as AndroidVideoSurfaceChooser gets smarter
  // about turning off overlays.  Either we should verify that it is not
  // breaking this use-case if it does so, or we should notify it that using
  // the overlay is required.
  if (force_video_overlays_)
    EnableOverlay();

  RequestOverlayInfoCB request_overlay_info_cb;
#if defined(OS_ANDROID)
  request_overlay_info_cb = BindToCurrentLoop(
      base::Bind(&WebMediaPlayerImpl::OnOverlayInfoRequested, AsWeakPtr()));
#endif
  return renderer_factory_selector_->GetCurrentFactory()->CreateRenderer(
      media_task_runner_, worker_task_runner_, audio_source_provider_.get(),
      compositor_, request_overlay_info_cb);
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      BindToCurrentLoop(base::Bind(
          &WebMediaPlayerImpl::OnEncryptedMediaInitData, AsWeakPtr()));

  if (renderer_factory_selector_->GetCurrentFactory()
          ->GetRequiredMediaResourceType() == MediaResource::Type::URL) {
    if (data_source_)
      loaded_url_ = data_source_->GetUrlAfterRedirects();

    // MediaPlayerRendererClient factory is the only factory that a
    // MediaResource::Type::URL for the moment. This might no longer be true
    // when we remove WebMediaPlayerCast.
    //
    // TODO(tguilbert/avayvod): Update this flag when removing |cast_impl_|.
    using_media_player_renderer_ = true;

    demuxer_.reset(
        new MediaUrlDemuxer(media_task_runner_, loaded_url_,
                            frame_->GetDocument().FirstPartyForCookies()));
    pipeline_controller_.Start(demuxer_.get(), this, false, false);
    return;
  }

  // Figure out which demuxer to use.
  if (load_type_ != kLoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

#if !defined(MEDIA_DISABLE_FFMPEG)
    Demuxer::MediaTracksUpdatedCB media_tracks_updated_cb =
        BindToCurrentLoop(base::Bind(
            &WebMediaPlayerImpl::OnFFmpegMediaTracksUpdated, AsWeakPtr()));

    demuxer_.reset(new FFmpegDemuxer(
        media_task_runner_, data_source_.get(), encrypted_media_init_data_cb,
        media_tracks_updated_cb, media_log_.get()));
#else
    OnError(PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
#endif
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    chunk_demuxer_ = new ChunkDemuxer(
        BindToCurrentLoop(
            base::Bind(&WebMediaPlayerImpl::OnDemuxerOpened, AsWeakPtr())),
        encrypted_media_init_data_cb, media_log_.get());
    demuxer_.reset(chunk_demuxer_);

    if (base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC)) {
      // base::Unretained is safe because |this| owns memory_pressure_listener_.
      memory_pressure_listener_ =
          base::MakeUnique<base::MemoryPressureListener>(base::Bind(
              &WebMediaPlayerImpl::OnMemoryPressure, base::Unretained(this)));
    }
  }

  // TODO(sandersd): FileSystem objects may also be non-static, but due to our
  // caching layer such situations are broken already. http://crbug.com/593159
  bool is_static = !chunk_demuxer_;
  bool is_streaming = IsStreaming();
  UMA_HISTOGRAM_BOOLEAN("Media.IsStreaming", is_streaming);

  // ... and we're ready to go!
  // TODO(sandersd): On Android, defer Start() if the tab is not visible.
  seeking_ = true;
  pipeline_controller_.Start(demuxer_.get(), this, is_streaming, is_static);
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
      data_source_->assume_fully_buffered() &&
      network_state_ == WebMediaPlayer::kNetworkStateLoading)
    SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);

  ready_state_ = state;
  highest_ready_state_ = std::max(highest_ready_state_, ready_state_);

  // Always notify to ensure client has the latest value.
  client_->ReadyStateChanged();
}

blink::WebAudioSourceProvider* WebMediaPlayerImpl::GetAudioSourceProvider() {
  return audio_source_provider_.get();
}

static void GetCurrentFrameAndSignal(VideoFrameCompositor* compositor,
                                     scoped_refptr<VideoFrame>* video_frame_out,
                                     base::WaitableEvent* event) {
  TRACE_EVENT0("media", "GetCurrentFrameAndSignal");
  *video_frame_out = compositor->GetCurrentFrameAndUpdateIfStale();
  event->Signal();
}

scoped_refptr<VideoFrame> WebMediaPlayerImpl::GetCurrentFrameFromCompositor() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::GetCurrentFrameFromCompositor");

  // Needed when the |main_task_runner_| and |compositor_task_runner_| are the
  // same to avoid deadlock in the Wait() below.
  if (compositor_task_runner_->BelongsToCurrentThread()) {
    scoped_refptr<VideoFrame> video_frame =
        compositor_->GetCurrentFrameAndUpdateIfStale();
    if (!video_frame) {
      return nullptr;
    }
    last_uploaded_frame_size_ = video_frame->natural_size();
    last_uploaded_frame_timestamp_ = video_frame->timestamp();
    return video_frame;
  }

  // Use a posted task and waitable event instead of a lock otherwise
  // WebGL/Canvas can see different content than what the compositor is seeing.
  scoped_refptr<VideoFrame> video_frame;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetCurrentFrameAndSignal, base::Unretained(compositor_),
                 &video_frame, &event));
  event.Wait();

  if (!video_frame) {
    return nullptr;
  }
  last_uploaded_frame_size_ = video_frame->natural_size();
  last_uploaded_frame_timestamp_ = video_frame->timestamp();
  return video_frame;
}

void WebMediaPlayerImpl::UpdatePlayState() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  bool is_remote = IsRemote();
  bool can_auto_suspend = true;
#else
  bool is_remote = false;
  bool can_auto_suspend = !disable_pipeline_auto_suspend_ && !IsStreaming();
#endif

  bool is_suspended = pipeline_controller_.IsSuspended();
  bool is_backgrounded = IsBackgroundedSuspendEnabled() && IsHidden();
  PlayState state = UpdatePlayState_ComputePlayState(
      is_remote, can_auto_suspend, is_suspended, is_backgrounded);
  SetDelegateState(state.delegate_state, state.is_idle);
  SetMemoryReportingState(state.is_memory_reporting_enabled);
  SetSuspendState(state.is_suspended || pending_suspend_resume_cycle_);
}

void WebMediaPlayerImpl::SetDelegateState(DelegateState new_state,
                                          bool is_idle) {
  DCHECK(delegate_);

  // Prevent duplicate delegate calls.
  // TODO(sandersd): Move this deduplication into the delegate itself.
  // TODO(sandersd): WebContentsObserverSanityChecker does not allow sending the
  // 'playing' IPC more than once in a row, even if the metadata has changed.
  // Figure out whether it should.
  bool has_audio = HasAudio() && !client_->IsAutoplayingMuted();
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
          media::DurationToMediaContentType(GetPipelineMediaDuration()));
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
  if (!suspend_enabled_) {
    DCHECK(!pipeline_controller_.IsSuspended());
    return;
  }

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
    pipeline_controller_.Suspend();
  } else {
    // When resuming, start the preroll attempt clock.
    if (preroll_attempt_pending_) {
      preroll_attempt_pending_ = false;
      preroll_attempt_start_time_ = tick_clock_->NowTicks();
    }
    pipeline_controller_.Resume();
  }
}

WebMediaPlayerImpl::PlayState
WebMediaPlayerImpl::UpdatePlayState_ComputePlayState(bool is_remote,
                                                     bool can_auto_suspend,
                                                     bool is_suspended,
                                                     bool is_backgrounded) {
  PlayState result;

  bool must_suspend = delegate_->IsFrameClosed();
  bool is_stale = delegate_->IsStale(delegate_id_);

  // This includes both data source (before pipeline startup) and pipeline
  // errors.
  bool has_error = IsNetworkStateError(network_state_);

  // After HaveFutureData, Blink will call play() if the state is not paused;
  // prior to this point |paused_| is not accurate.
  bool have_future_data =
      highest_ready_state_ >= WebMediaPlayer::kReadyStateHaveFutureData;

  // Background suspend is only enabled for paused players.
  // In the case of players with audio the session should be kept.
  bool background_suspended =
      can_auto_suspend && is_backgrounded && paused_ && have_future_data;

  // Idle suspension is allowed prior to have future data since there exist
  // mechanisms to exit the idle state when the player is capable of reaching
  // the have future data state; see didLoadingProgress().
  //
  // TODO(sandersd): Make the delegate suspend idle players immediately when
  // hidden.
  bool idle_suspended =
      can_auto_suspend && is_stale && paused_ && !seeking_ && !overlay_enabled_;

  // If we're already suspended, see if we can wait for user interaction. Prior
  // to HaveFutureData, we require |is_stale| to remain suspended. |is_stale|
  // will be cleared when we receive data which may take us to HaveFutureData.
  bool can_stay_suspended =
      (is_stale || have_future_data) && is_suspended && paused_ && !seeking_;

  // Combined suspend state.
  result.is_suspended = is_remote || must_suspend || idle_suspended ||
                        background_suspended || can_stay_suspended;

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
  // (requires |have_future_data|).
  //
  // |alive| indicates if the player should be present (not |GONE|) to the
  // delegate, either paused or playing. The following must be true for the
  // player:
  //   - |have_future_data|, since we need to know whether we are paused to
  //     correctly configure the session and also because the tracks and
  //     duration are passed to DidPlay(),
  //   - |is_remote| is false as remote playback is not handled by the delegate,
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
      IsBackgroundedSuspendEnabled() && !IsResumeBackgroundVideosEnabled() &&
      is_backgrounded && HasVideo();
  bool can_play = !has_error && !is_remote && have_future_data;
  bool has_remote_controls =
      HasAudio() && !backgrounded_video_has_no_remote_controls;
  bool alive = can_play && !must_suspend &&
               (!background_suspended || has_remote_controls);
  if (!alive) {
    result.delegate_state = DelegateState::GONE;
    result.is_idle = delegate_->IsIdle(delegate_id_);
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
  result.is_memory_reporting_enabled =
      can_play && !result.is_suspended && (!paused_ || seeking_);

  return result;
}

void WebMediaPlayerImpl::ReportMemoryUsage() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // About base::Unretained() usage below: We destroy |demuxer_| on the main
  // thread.  Before that, however, ~WebMediaPlayerImpl() posts a task to the
  // media thread and waits for it to finish.  Hence, the GetMemoryUsage() task
  // posted here must finish earlier.

  if (demuxer_) {
    base::PostTaskAndReplyWithResult(
        media_task_runner_.get(), FROM_HERE,
        base::Bind(&Demuxer::GetMemoryUsage, base::Unretained(demuxer_.get())),
        base::Bind(&WebMediaPlayerImpl::FinishMemoryUsageReport, AsWeakPtr()));
  } else {
    FinishMemoryUsageReport(0);
  }
}

void WebMediaPlayerImpl::FinishMemoryUsageReport(int64_t demuxer_memory_usage) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  const PipelineStatistics stats = GetPipelineStatistics();
  const int64_t data_source_memory_usage =
      data_source_ ? data_source_->GetMemoryUsage() : 0;

  // If we have video and no video memory usage, assume the VideoFrameCompositor
  // is holding onto the last frame after we've suspended the pipeline; which
  // thus reports zero memory usage from the video renderer.
  //
  // Technically this should use the coded size, but that requires us to hop to
  // the compositor to get and byte-perfect accuracy isn't important here.
  const int64_t video_memory_usage =
      stats.video_memory_usage +
      (pipeline_metadata_.has_video && !stats.video_memory_usage
           ? VideoFrame::AllocationSize(PIXEL_FORMAT_YV12,
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
      !pipeline_controller_.IsSuspended() || !HasAudio()) {
    return;
  }

#if defined(OS_ANDROID)
  // Remote players will be suspended and locally paused.
  if (IsRemote())
    return;
#endif

  // Idle timeout chosen arbitrarily.
  background_pause_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                                this, &WebMediaPlayerImpl::OnPause);
}

void WebMediaPlayerImpl::CreateWatchTimeReporter() {
  if (!HasVideo() && !HasAudio())
    return;

  // Create the watch time reporter and synchronize its initial state.
  watch_time_reporter_.reset(
      new WatchTimeReporter(HasAudio(), HasVideo(), !!chunk_demuxer_,
                            is_encrypted_, embedded_media_experience_enabled_,
                            media_log_.get(), pipeline_metadata_.natural_size,
                            base::Bind(&GetCurrentTimeInternal, this)));
  watch_time_reporter_->OnVolumeChange(volume_);
  if (delegate_->IsFrameHidden())
    watch_time_reporter_->OnHidden();
  else
    watch_time_reporter_->OnShown();
  if (client_->HasNativeControls())
    watch_time_reporter_->OnNativeControlsEnabled();
  else
    watch_time_reporter_->OnNativeControlsDisabled();
}

bool WebMediaPlayerImpl::IsHidden() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return delegate_->IsFrameHidden() && !delegate_->IsFrameClosed();
}

bool WebMediaPlayerImpl::IsStreaming() const {
  return data_source_ && data_source_->IsStreaming();
}

bool WebMediaPlayerImpl::DoesOverlaySupportMetadata() const {
  return pipeline_metadata_.video_rotation == VIDEO_ROTATION_0;
}

void WebMediaPlayerImpl::ActivateViewportIntersectionMonitoring(bool activate) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  client_->ActivateViewportIntersectionMonitoring(activate);
}

bool WebMediaPlayerImpl::ShouldPauseVideoWhenHidden() const {
  // If suspending background video, pause any video that's not remoted or
  // not unlocked to play in the background.
  if (IsBackgroundedSuspendEnabled()) {
    if (!HasVideo())
      return false;

#if defined(OS_ANDROID)
    if (IsRemote())
      return false;
#endif

    return !HasAudio() || (IsResumeBackgroundVideosEnabled() &&
                           video_locked_when_paused_when_hidden_);
  }

  // Otherwise only pause if the optimization is on and it's a video-only
  // optimization candidate.
  return IsBackgroundVideoPauseOptimizationEnabled() && !HasAudio() &&
         IsBackgroundOptimizationCandidate();
}

bool WebMediaPlayerImpl::ShouldDisableVideoWhenHidden() const {
  // This optimization is behind the flag on all platforms.
  if (!IsBackgroundVideoTrackOptimizationEnabled())
    return false;

  // Disable video track only for players with audio that match the criteria for
  // being optimized.
  return HasAudio() && IsBackgroundOptimizationCandidate();
}

bool WebMediaPlayerImpl::IsBackgroundOptimizationCandidate() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

#if defined(OS_ANDROID)  // WMPI_CAST
  // Don't optimize players being Cast.
  if (IsRemote())
    return false;

  // Video-only players are always optimized (paused) on Android.
  // Don't check the keyframe distance and duration.
  if (!HasAudio() && HasVideo())
    return true;
#endif  // defined(OS_ANDROID)

  // Don't optimize audio-only or streaming players.
  if (!HasVideo() || IsStreaming())
    return false;

  // Videos shorter than the maximum allowed keyframe distance can be optimized.
  base::TimeDelta duration = GetPipelineMediaDuration();
  base::TimeDelta max_keyframe_distance =
      (load_type_ == kLoadTypeMediaSource)
          ? max_keyframe_distance_to_disable_background_video_mse_
          : max_keyframe_distance_to_disable_background_video_;
  if (duration < max_keyframe_distance)
    return true;

  // Otherwise, only optimize videos with shorter average keyframe distance.
  PipelineStatistics stats = GetPipelineStatistics();
  return stats.video_keyframe_distance_average < max_keyframe_distance;
}

void WebMediaPlayerImpl::UpdateBackgroundVideoOptimizationState() {
  if (IsHidden()) {
    if (ShouldPauseVideoWhenHidden())
      PauseVideoIfNeeded();
    else
      DisableVideoTrackIfNeeded();
  } else {
    EnableVideoTrackIfNeeded();
  }
}

void WebMediaPlayerImpl::PauseVideoIfNeeded() {
  DCHECK(IsHidden());

  // Don't pause video while the pipeline is stopped, resuming or seeking.
  // Also if the video is paused already.
  if (!pipeline_controller_.IsPipelineRunning() || is_pipeline_resuming_ ||
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
  if (!pipeline_controller_.IsPipelineRunning() || is_pipeline_resuming_ ||
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
      pipeline_controller_.GetStatistics());
}

void WebMediaPlayerImpl::SetPipelineMediaDurationForTest(
    base::TimeDelta duration) {
  pipeline_media_duration_for_test_ = base::make_optional(duration);
}

base::TimeDelta WebMediaPlayerImpl::GetPipelineMediaDuration() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return pipeline_media_duration_for_test_.value_or(
      pipeline_controller_.GetMediaDuration());
}

void WebMediaPlayerImpl::ReportTimeFromForegroundToFirstFrame(
    base::TimeTicks foreground_time,
    base::TimeTicks new_frame_time) {
  base::TimeDelta time_to_first_frame = new_frame_time - foreground_time;
  if (HasAudio()) {
    UMA_HISTOGRAM_TIMES(
        "Media.Video.TimeFromForegroundToFirstFrame.DisableTrack",
        time_to_first_frame);
  } else {
    UMA_HISTOGRAM_TIMES("Media.Video.TimeFromForegroundToFirstFrame.Paused",
                        time_to_first_frame);
  }
}
void WebMediaPlayerImpl::SwitchRenderer(bool is_rendered_remotely) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  disable_pipeline_auto_suspend_ = is_rendered_remotely;
  ScheduleRestart();
  if (client_) {
    if (is_rendered_remotely)
      client_->MediaRemotingStarted();
    else
      client_->MediaRemotingStopped();
  }
}

void WebMediaPlayerImpl::RecordUnderflowDuration(base::TimeDelta duration) {
  DCHECK(data_source_ || chunk_demuxer_);

  if (data_source_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration", duration);
  else
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration.MSE", duration);

  if (is_encrypted_)
    UMA_HISTOGRAM_TIMES("Media.UnderflowDuration.EME", duration);
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

void WebMediaPlayerImpl::SetTickClockForTest(base::TickClock* tick_clock) {
  tick_clock_.reset(tick_clock);
  buffered_data_source_host_.SetTickClockForTest(tick_clock);
}

}  // namespace media
