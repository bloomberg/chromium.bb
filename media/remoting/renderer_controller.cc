// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/media_features.h"
#include "media/remoting/remoting_cdm.h"
#include "media/remoting/remoting_cdm_context.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

namespace media {
namespace remoting {

namespace {

// The duration to delay the start of media remoting to ensure all preconditions
// are held stable before switching to media remoting.
constexpr base::TimeDelta kDelayedStart = base::TimeDelta::FromSeconds(5);

constexpr int kPixelPerSec4K = 3840 * 2160 * 30;  // 4k 30fps.
constexpr int kPixelPerSec2K = 1920 * 1080 * 30;  // 1080p 30fps.

// The minimum media element duration that is allowed for media remoting.
// Frequent switching into and out of media remoting for short-duration media
// can feel "janky" to the user.
constexpr double kMinRemotingMediaDurationInSec = 60;

StopTrigger GetStopTrigger(mojom::RemotingStopReason reason) {
  switch (reason) {
    case mojom::RemotingStopReason::ROUTE_TERMINATED:
      return ROUTE_TERMINATED;
    case mojom::RemotingStopReason::SOURCE_GONE:
      return MEDIA_ELEMENT_DESTROYED;
    case mojom::RemotingStopReason::MESSAGE_SEND_FAILED:
      return MESSAGE_SEND_FAILED;
    case mojom::RemotingStopReason::DATA_SEND_FAILED:
      return DATA_SEND_FAILED;
    case mojom::RemotingStopReason::UNEXPECTED_FAILURE:
      return UNEXPECTED_FAILURE;
    case mojom::RemotingStopReason::SERVICE_GONE:
      return SERVICE_GONE;
    case mojom::RemotingStopReason::USER_DISABLED:
      return USER_DISABLED;
    case mojom::RemotingStopReason::LOCAL_PLAYBACK:
      // This RemotingStopReason indicates the RendererController initiated the
      // session shutdown in the immediate past, and the trigger for that should
      // have already been recorded in the metrics. Here, this is just duplicate
      // feedback from the sink for that same event. Return UNKNOWN_STOP_TRIGGER
      // because this reason can not be a stop trigger and it would be a logic
      // flaw for this value to be recorded in the metrics.
      return UNKNOWN_STOP_TRIGGER;
  }

  return UNKNOWN_STOP_TRIGGER;  // To suppress compiler warning on Windows.
}

MediaObserverClient::ReasonToSwitchToLocal GetSwitchReason(
    StopTrigger stop_trigger) {
  switch (stop_trigger) {
    case FRAME_DROP_RATE_HIGH:
    case PACING_TOO_SLOWLY:
      return MediaObserverClient::ReasonToSwitchToLocal::POOR_PLAYBACK_QUALITY;
    case EXITED_FULLSCREEN:
    case BECAME_AUXILIARY_CONTENT:
    case DISABLED_BY_PAGE:
    case USER_DISABLED:
    case UNKNOWN_STOP_TRIGGER:
      return MediaObserverClient::ReasonToSwitchToLocal::NORMAL;
    case UNSUPPORTED_AUDIO_CODEC:
    case UNSUPPORTED_VIDEO_CODEC:
    case UNSUPPORTED_AUDIO_AND_VIDEO_CODECS:
    case DECRYPTION_ERROR:
    case RECEIVER_INITIALIZE_FAILED:
    case RECEIVER_PIPELINE_ERROR:
    case PEERS_OUT_OF_SYNC:
    case RPC_INVALID:
    case DATA_PIPE_CREATE_ERROR:
    case MOJO_PIPE_ERROR:
    case MESSAGE_SEND_FAILED:
    case DATA_SEND_FAILED:
    case UNEXPECTED_FAILURE:
      return MediaObserverClient::ReasonToSwitchToLocal::PIPELINE_ERROR;
    case ROUTE_TERMINATED:
    case MEDIA_ELEMENT_DESTROYED:
    case START_RACE:
    case SERVICE_GONE:
      return MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED;
  }

  // To suppress compiler warning on Windows.
  return MediaObserverClient::ReasonToSwitchToLocal::ROUTE_TERMINATED;
}

}  // namespace

RendererController::RendererController(scoped_refptr<SharedSession> session)
    : session_(std::move(session)),
      clock_(base::DefaultTickClock::GetInstance()),
      weak_factory_(this) {
  session_->AddClient(this);
}

RendererController::~RendererController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics_recorder_.WillStopSession(MEDIA_ELEMENT_DESTROYED);
  session_->RemoveClient(this);
}

void RendererController::OnStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    if (remote_rendering_started_) {
      metrics_recorder_.DidStartSession();
      DCHECK(client_);
      client_->SwitchToRemoteRenderer(session_->sink_name());
    } else {
      session_->StopRemoting(this);
    }
  } else {
    VLOG(1) << "Failed to start remoting.";
    remote_rendering_started_ = false;
    metrics_recorder_.WillStopSession(START_RACE);
  }
}

void RendererController::OnSessionStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateFromSessionState(SINK_AVAILABLE,
                         GetStopTrigger(session_->get_last_stop_reason()));
}

void RendererController::UpdateFromSessionState(StartTrigger start_trigger,
                                                StopTrigger stop_trigger) {
  VLOG(1) << "UpdateFromSessionState: " << session_->state();
  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

bool RendererController::IsRemoteSinkAvailable() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (session_->state()) {
    case SharedSession::SESSION_CAN_START:
    case SharedSession::SESSION_STARTING:
    case SharedSession::SESSION_STARTED:
      return true;
    case SharedSession::SESSION_UNAVAILABLE:
    case SharedSession::SESSION_STOPPING:
    case SharedSession::SESSION_PERMANENTLY_STOPPED:
      return false;
  }

  NOTREACHED();
  return false;  // To suppress compiler warning on Windows.
}

void RendererController::OnBecameDominantVisibleContent(bool is_dominant) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_dominant_content_ == is_dominant)
    return;
  is_dominant_content_ = is_dominant;
  // Reset the errors when the media element stops being the dominant visible
  // content in the tab.
  if (!is_dominant_content_)
    encountered_renderer_fatal_error_ = false;
  UpdateAndMaybeSwitch(BECAME_DOMINANT_CONTENT, BECAME_AUXILIARY_CONTENT);
}

void RendererController::OnSetCdm(CdmContext* cdm_context) {
  VLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  auto* remoting_cdm_context = RemotingCdmContext::From(cdm_context);
  if (!remoting_cdm_context)
    return;

  session_->RemoveClient(this);
  session_ = remoting_cdm_context->GetSharedSession();
  session_->AddClient(this);
  UpdateFromSessionState(CDM_READY, DECRYPTION_ERROR);
}

void RendererController::OnRemotePlaybackDisabled(bool disabled) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_remote_playback_disabled_ = disabled;
  metrics_recorder_.OnRemotePlaybackDisabled(disabled);
  UpdateAndMaybeSwitch(ENABLED_BY_PAGE, DISABLED_BY_PAGE);
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
base::WeakPtr<RpcBroker> RendererController::GetRpcBroker() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return session_->rpc_broker()->GetWeakPtr();
}
#endif

void RendererController::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    const SharedSession::DataPipeStartCallback& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  session_->StartDataPipe(std::move(audio_data_pipe),
                          std::move(video_data_pipe), done_callback);
}

void RendererController::OnMetadataChanged(const PipelineMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool was_audio_codec_supported = has_audio() && IsAudioCodecSupported();
  const bool was_video_codec_supported = has_video() && IsVideoCodecSupported();
  pipeline_metadata_ = metadata;
  const bool is_audio_codec_supported = has_audio() && IsAudioCodecSupported();
  const bool is_video_codec_supported = has_video() && IsVideoCodecSupported();
  metrics_recorder_.OnPipelineMetadataChanged(metadata);

  is_encrypted_ = false;
  if (has_video())
    is_encrypted_ |= metadata.video_decoder_config.is_encrypted();
  if (has_audio())
    is_encrypted_ |= metadata.audio_decoder_config.is_encrypted();

  StartTrigger start_trigger = UNKNOWN_START_TRIGGER;
  if (!was_audio_codec_supported && is_audio_codec_supported)
    start_trigger = SUPPORTED_AUDIO_CODEC;
  if (!was_video_codec_supported && is_video_codec_supported) {
    start_trigger = start_trigger == SUPPORTED_AUDIO_CODEC
                        ? SUPPORTED_AUDIO_AND_VIDEO_CODECS
                        : SUPPORTED_VIDEO_CODEC;
  }
  StopTrigger stop_trigger = UNKNOWN_STOP_TRIGGER;
  if (was_audio_codec_supported && !is_audio_codec_supported)
    stop_trigger = UNSUPPORTED_AUDIO_CODEC;
  if (was_video_codec_supported && !is_video_codec_supported) {
    stop_trigger = stop_trigger == UNSUPPORTED_AUDIO_CODEC
                       ? UNSUPPORTED_AUDIO_AND_VIDEO_CODECS
                       : UNSUPPORTED_VIDEO_CODEC;
  }

  UpdateRemotePlaybackAvailabilityMonitoringState();

  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

void RendererController::OnDataSourceInitialized(
    const GURL& url_after_redirects) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (url_after_redirects == url_after_redirects_)
    return;

  // TODO(avayvod): Does WMPI update MediaObserver when metadata becomes
  // invalid or should we reset it here?
  url_after_redirects_ = url_after_redirects;

  UpdateRemotePlaybackAvailabilityMonitoringState();
}

void RendererController::UpdateRemotePlaybackAvailabilityMonitoringState() {
  if (!client_)
    return;

// Currently RemotePlayback-initated media remoting only supports URL flinging
// thus the source is supported when the URL is either http or https, video and
// audio codecs are supported by the remote playback device; HLS is playable by
// Chrome on Android (which is not detected by the pipeline metadata atm).
#if defined(OS_ANDROID)
  // TODO(tguilbert): Detect the presence of HLS based on demuxing results,
  // rather than the URL string. See crbug.com/663503.
  bool is_hls = MediaCodecUtil::IsHLSURL(url_after_redirects_);
#else
  bool is_hls = false;
#endif
  // TODO(avayvod): add a check for CORS.
  bool is_source_supported = url_after_redirects_.has_scheme() &&
                             (url_after_redirects_.SchemeIs("http") ||
                              url_after_redirects_.SchemeIs("https")) &&
                             (is_hls || IsAudioOrVideoSupported());

  client_->UpdateRemotePlaybackCompatibility(is_source_supported);
}

bool RendererController::IsVideoCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  switch (pipeline_metadata_.video_decoder_config.codec()) {
    case VideoCodec::kCodecH264:
      return session_->HasVideoCapability(
          mojom::RemotingSinkVideoCapability::CODEC_H264);
    case VideoCodec::kCodecVP8:
      return session_->HasVideoCapability(
          mojom::RemotingSinkVideoCapability::CODEC_VP8);
    case VideoCodec::kCodecVP9:
      return session_->HasVideoCapability(
          mojom::RemotingSinkVideoCapability::CODEC_VP9);
    case VideoCodec::kCodecHEVC:
      return session_->HasVideoCapability(
          mojom::RemotingSinkVideoCapability::CODEC_HEVC);
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << pipeline_metadata_.video_decoder_config.codec();
      return false;
  }
}

bool RendererController::IsAudioCodecSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kCodecAAC:
      return session_->HasAudioCapability(
          mojom::RemotingSinkAudioCapability::CODEC_AAC);
    case AudioCodec::kCodecOpus:
      return session_->HasAudioCapability(
          mojom::RemotingSinkAudioCapability::CODEC_OPUS);
    case AudioCodec::kCodecMP3:
    case AudioCodec::kCodecPCM:
    case AudioCodec::kCodecVorbis:
    case AudioCodec::kCodecFLAC:
    case AudioCodec::kCodecAMR_NB:
    case AudioCodec::kCodecAMR_WB:
    case AudioCodec::kCodecPCM_MULAW:
    case AudioCodec::kCodecGSM_MS:
    case AudioCodec::kCodecPCM_S16BE:
    case AudioCodec::kCodecPCM_S24BE:
    case AudioCodec::kCodecEAC3:
    case AudioCodec::kCodecPCM_ALAW:
    case AudioCodec::kCodecALAC:
    case AudioCodec::kCodecAC3:
      return session_->HasAudioCapability(
          mojom::RemotingSinkAudioCapability::CODEC_BASELINE_SET);
    default:
      VLOG(2) << "Remoting does not support audio codec: "
              << pipeline_metadata_.audio_decoder_config.codec();
      return false;
  }
}

void RendererController::OnPlaying() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = false;
  UpdateAndMaybeSwitch(PLAY_COMMAND, UNKNOWN_STOP_TRIGGER);
}

void RendererController::OnPaused() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = true;
  // Cancel the start if in the middle of delayed start.
  CancelDelayedStart();
}

bool RendererController::CanBeRemoting() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_) {
    DCHECK(!remote_rendering_started_);
    return false;  // No way to switch to the remoting renderer.
  }

  const SharedSession::SessionState state = session_->state();
  if (is_encrypted_) {
    // Due to technical limitations when playing encrypted content, once a
    // remoting session has been started, playback cannot be resumed locally
    // without reloading the page, so leave the CourierRenderer in-place to
    // avoid having the default renderer attempt and fail to play the content.
    //
    // TODO(miu): Revisit this once more of the encrypted-remoting impl is
    // in-place. For example, this will prevent metrics from recording session
    // stop reasons.
    return state == SharedSession::SESSION_STARTED ||
           state == SharedSession::SESSION_STOPPING ||
           state == SharedSession::SESSION_PERMANENTLY_STOPPED;
  }

  if (permanently_disable_remoting_)
    return false;

  switch (state) {
    case SharedSession::SESSION_UNAVAILABLE:
      return false;  // Cannot remote media without a remote sink.
    case SharedSession::SESSION_CAN_START:
    case SharedSession::SESSION_STARTING:
    case SharedSession::SESSION_STARTED:
      break;  // Media remoting is possible, assuming other requirments are met.
    case SharedSession::SESSION_STOPPING:
    case SharedSession::SESSION_PERMANENTLY_STOPPED:
      return false;  // Use local rendering after stopping remoting.
  }

  if (!IsAudioOrVideoSupported())
    return false;

  if (is_remote_playback_disabled_)
    return false;

  if (client_->Duration() <= kMinRemotingMediaDurationInSec)
    return false;

  return true;
}

bool RendererController::IsAudioOrVideoSupported() const {
  if ((!has_audio() && !has_video()) ||
      (has_video() && !IsVideoCodecSupported()) ||
      (has_audio() && !IsAudioCodecSupported())) {
    return false;
  }

  return true;
}

void RendererController::UpdateAndMaybeSwitch(StartTrigger start_trigger,
                                              StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool should_be_remoting = CanBeRemoting();
  if (!is_encrypted_ && client_)
    client_->ActivateViewportIntersectionMonitoring(should_be_remoting);

  // Normally, being the dominant visible content is the signal that starts
  // remote rendering. However, current technical limitations require encrypted
  // content be remoted without waiting for a user signal.
  if (!is_encrypted_)
    should_be_remoting &=
        (is_dominant_content_ && !encountered_renderer_fatal_error_);

  if ((remote_rendering_started_ ||
       delayed_start_stability_timer_.IsRunning()) == should_be_remoting)
    return;

  DCHECK(client_);

  if (is_encrypted_) {
    DCHECK(should_be_remoting);
    StartRemoting(start_trigger);
    return;
  }

  // Only switch to remoting when media is playing. Since the renderer is
  // created when video starts loading/playing, receiver will display a black
  // screen before video starts playing if switching to remoting when paused.
  // Thus, the user experience is improved by not starting remoting until
  // playback resumes.
  if (should_be_remoting && is_paused_)
    return;

  if (should_be_remoting) {
    WaitForStabilityBeforeStart(start_trigger);
  } else if (delayed_start_stability_timer_.IsRunning()) {
    DCHECK(!remote_rendering_started_);
    CancelDelayedStart();
  } else {
    remote_rendering_started_ = false;
    // For encrypted content, it's only valid to switch to remoting renderer,
    // and never back to the local renderer. The RemotingCdmController will
    // force-stop the session when remoting has ended; so no need to call
    // StopRemoting() from here.
    DCHECK(!is_encrypted_);
    DCHECK_NE(UNKNOWN_STOP_TRIGGER, stop_trigger);
    metrics_recorder_.WillStopSession(stop_trigger);
    client_->SwitchToLocalRenderer(GetSwitchReason(stop_trigger));
    VLOG(2) << "Request to stop remoting: stop_trigger=" << stop_trigger;
    session_->StopRemoting(this);
  }
}

void RendererController::WaitForStabilityBeforeStart(
    StartTrigger start_trigger) {
  DCHECK(!delayed_start_stability_timer_.IsRunning());
  DCHECK(!remote_rendering_started_);
  DCHECK(!is_encrypted_);
  delayed_start_stability_timer_.Start(
      FROM_HERE, kDelayedStart,
      base::Bind(&RendererController::OnDelayedStartTimerFired,
                 base::Unretained(this), start_trigger,
                 client_->DecodedFrameCount(), clock_->NowTicks()));
}

void RendererController::CancelDelayedStart() {
  delayed_start_stability_timer_.Stop();
}

void RendererController::OnDelayedStartTimerFired(
    StartTrigger start_trigger,
    unsigned decoded_frame_count_before_delay,
    base::TimeTicks delayed_start_time) {
  DCHECK(is_dominant_content_);
  DCHECK(!remote_rendering_started_);
  DCHECK(!is_encrypted_);

  base::TimeDelta elapsed = clock_->NowTicks() - delayed_start_time;
  DCHECK(!elapsed.is_zero());
  if (has_video()) {
    const double frame_rate =
        (client_->DecodedFrameCount() - decoded_frame_count_before_delay) /
        elapsed.InSecondsF();
    const double pixel_per_sec =
        frame_rate * pipeline_metadata_.natural_size.GetArea();
    if ((pixel_per_sec > kPixelPerSec4K) ||
        ((pixel_per_sec > kPixelPerSec2K) &&
         !session_->HasVideoCapability(
             mojom::RemotingSinkVideoCapability::SUPPORT_4K))) {
      VLOG(1) << "Media remoting is not supported: frame_rate = " << frame_rate
              << " resolution = " << pipeline_metadata_.natural_size.ToString();
      permanently_disable_remoting_ = true;
      return;
    }
  }

  StartRemoting(start_trigger);
}

void RendererController::StartRemoting(StartTrigger start_trigger) {
  DCHECK(client_);
  remote_rendering_started_ = true;
  if (session_->state() == SharedSession::SESSION_PERMANENTLY_STOPPED) {
    client_->SwitchToRemoteRenderer(session_->sink_name());
    return;
  }
  DCHECK_NE(UNKNOWN_START_TRIGGER, start_trigger);
  metrics_recorder_.WillStartSession(start_trigger);
  // |MediaObserverClient::SwitchToRemoteRenderer()| will be called after
  // remoting is started successfully.
  session_->StartRemoting(this);
}

void RendererController::OnRendererFatalError(StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not act on errors caused by things like Mojo pipes being closed during
  // shutdown.
  if (!remote_rendering_started_)
    return;

  encountered_renderer_fatal_error_ = true;
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, stop_trigger);
}

void RendererController::SetClient(MediaObserverClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client);
  DCHECK(!client_);

  client_ = client;
  if (!is_encrypted_)
    client_->ActivateViewportIntersectionMonitoring(CanBeRemoting());
}

}  // namespace remoting
}  // namespace media
