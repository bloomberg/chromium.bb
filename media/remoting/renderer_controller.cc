// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/remoting/remoting_cdm.h"
#include "media/remoting/remoting_cdm_context.h"

namespace media {
namespace remoting {

RendererController::RendererController(scoped_refptr<SharedSession> session)
    : session_(std::move(session)), weak_factory_(this) {
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
    VLOG(1) << "Remoting started successively.";
    if (remote_rendering_started_) {
      metrics_recorder_.DidStartSession();
      DCHECK(client_);
      client_->SwitchRenderer(true);
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
  UpdateFromSessionState(SINK_AVAILABLE, ROUTE_TERMINATED);
}

void RendererController::UpdateFromSessionState(StartTrigger start_trigger,
                                                StopTrigger stop_trigger) {
  VLOG(1) << "UpdateFromSessionState: " << session_->state();
  if (client_)
    client_->ActivateViewportIntersectionMonitoring(IsRemoteSinkAvailable());

  UpdateInterstitial(base::nullopt);
  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

bool RendererController::IsRemoteSinkAvailable() {
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

void RendererController::OnEnteredFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = true;
  // See notes in OnBecameDominantVisibleContent() for why this is forced:
  is_dominant_content_ = true;
  UpdateAndMaybeSwitch(ENTERED_FULLSCREEN, UNKNOWN_STOP_TRIGGER);
}

void RendererController::OnExitedFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = false;
  // See notes in OnBecameDominantVisibleContent() for why this is forced:
  is_dominant_content_ = false;
  UpdateAndMaybeSwitch(UNKNOWN_START_TRIGGER, EXITED_FULLSCREEN);
}

void RendererController::OnBecameDominantVisibleContent(bool is_dominant) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Two scenarios where "dominance" status mixes with fullscreen transitions:
  //
  //   1. Just before/after entering fullscreen, the element will, of course,
  //      become the dominant on-screen content via automatic page layout.
  //   2. Just before/after exiting fullscreen, the element may or may not
  //      shrink in size enough to become non-dominant. However, exiting
  //      fullscreen was caused by a user action that explicitly indicates a
  //      desire to exit remoting, so even if the element is still dominant,
  //      remoting should be shut down.
  //
  // Thus, to achieve the desired behaviors, |is_dominant_content_| is force-set
  // in OnEnteredFullscreen() and OnExitedFullscreen(), and changes to it here
  // are ignored while in fullscreen.
  if (is_fullscreen_)
    return;

  is_dominant_content_ = is_dominant;
  UpdateAndMaybeSwitch(BECAME_DOMINANT_CONTENT, BECAME_AUXILIARY_CONTENT);
}

void RendererController::OnSetCdm(CdmContext* cdm_context) {
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

void RendererController::OnSetPoster(const GURL& poster_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (poster_url != poster_url_) {
    poster_url_ = poster_url;
    if (poster_url_.is_empty())
      UpdateInterstitial(SkBitmap());
    else
      DownloadPosterImage();
  }
}

base::WeakPtr<RpcBroker> RendererController::GetRpcBroker() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return session_->rpc_broker()->GetWeakPtr();
}

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

  const gfx::Size old_size = pipeline_metadata_.natural_size;
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

  if (pipeline_metadata_.natural_size != old_size)
    UpdateInterstitial(base::nullopt);

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
  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

bool RendererController::IsVideoCodecSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video());

  switch (pipeline_metadata_.video_decoder_config.codec()) {
    case VideoCodec::kCodecH264:
    case VideoCodec::kCodecVP8:
      return true;
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << pipeline_metadata_.video_decoder_config.codec();
      return false;
  }
}

bool RendererController::IsAudioCodecSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_audio());

  switch (pipeline_metadata_.audio_decoder_config.codec()) {
    case AudioCodec::kCodecAAC:
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
    case AudioCodec::kCodecOpus:
    case AudioCodec::kCodecEAC3:
    case AudioCodec::kCodecPCM_ALAW:
    case AudioCodec::kCodecALAC:
    case AudioCodec::kCodecAC3:
      return true;
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
}

bool RendererController::ShouldBeRemoting() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_) {
    DCHECK(!remote_rendering_started_);
    return false;  // No way to switch to the remoting renderer.
  }

  const SharedSession::SessionState state = session_->state();
  if (is_encrypted_) {
    // Due to technical limitations when playing encrypted content, once a
    // remoting session has been started, always return true here to indicate
    // that the CourierRenderer should continue to be used. In the stopped
    // states, CourierRenderer will display an interstitial to notify the user
    // that local rendering cannot be resumed.
    //
    // TODO(miu): Revisit this once more of the encrypted-remoting impl is
    // in-place. For example, this will prevent metrics from recording session
    // stop reasons.
    return state == SharedSession::SESSION_STARTED ||
           state == SharedSession::SESSION_STOPPING ||
           state == SharedSession::SESSION_PERMANENTLY_STOPPED;
  }

  if (encountered_renderer_fatal_error_)
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

  switch (session_->sink_capabilities()) {
    case mojom::RemotingSinkCapabilities::NONE:
      return false;
    case mojom::RemotingSinkCapabilities::RENDERING_ONLY:
    case mojom::RemotingSinkCapabilities::CONTENT_DECRYPTION_AND_RENDERING:
      break;  // The sink is capable of remote rendering.
  }

  if ((!has_audio() && !has_video()) ||
      (has_video() && !IsVideoCodecSupported()) ||
      (has_audio() && !IsAudioCodecSupported())) {
    return false;
  }

  if (is_remote_playback_disabled_)
    return false;

  // Normally, entering fullscreen or being the dominant visible content is the
  // signal that starts remote rendering. However, current technical limitations
  // require encrypted content be remoted without waiting for a user signal.
  return is_fullscreen_ || is_dominant_content_;
}

void RendererController::UpdateAndMaybeSwitch(StartTrigger start_trigger,
                                              StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool should_be_remoting = ShouldBeRemoting();

  if (remote_rendering_started_ == should_be_remoting)
    return;

  // Only switch to remoting when media is playing. Since the renderer is
  // created when video starts loading/playing, receiver will display a black
  // screen before video starts playing if switching to remoting when paused.
  // Thus, the user experience is improved by not starting remoting until
  // playback resumes.
  if (should_be_remoting && is_paused_)
    return;

  // Switch between local renderer and remoting renderer.
  remote_rendering_started_ = should_be_remoting;

  DCHECK(client_);
  if (remote_rendering_started_) {
    if (session_->state() == SharedSession::SESSION_PERMANENTLY_STOPPED) {
      client_->SwitchRenderer(true);
      return;
    }
    DCHECK_NE(start_trigger, UNKNOWN_START_TRIGGER);
    metrics_recorder_.WillStartSession(start_trigger);
    // |MediaObserverClient::SwitchRenderer()| will be called after remoting is
    // started successfully.
    session_->StartRemoting(this);
  } else {
    // For encrypted content, it's only valid to switch to remoting renderer,
    // and never back to the local renderer. The RemotingCdmController will
    // force-stop the session when remoting has ended; so no need to call
    // StopRemoting() from here.
    DCHECK(!is_encrypted_);
    DCHECK_NE(stop_trigger, UNKNOWN_STOP_TRIGGER);
    metrics_recorder_.WillStopSession(stop_trigger);
    // Update the interstitial one last time before switching back to the local
    // Renderer.
    UpdateInterstitial(base::nullopt);
    client_->SwitchRenderer(false);
    session_->StopRemoting(this);
  }
}

void RendererController::SetShowInterstitialCallback(
    const ShowInterstitialCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  show_interstitial_cb_ = cb;
  UpdateInterstitial(SkBitmap());
  if (!poster_url_.is_empty())
    DownloadPosterImage();
}

void RendererController::SetDownloadPosterCallback(
    const DownloadPosterCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(download_poster_cb_.is_null());
  download_poster_cb_ = cb;
  if (!poster_url_.is_empty())
    DownloadPosterImage();
}

void RendererController::UpdateInterstitial(
    const base::Optional<SkBitmap>& image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (show_interstitial_cb_.is_null())
    return;

  InterstitialType type = InterstitialType::BETWEEN_SESSIONS;
  switch (remote_rendering_started_ ? session_->state()
                                    : SharedSession::SESSION_STOPPING) {
    case SharedSession::SESSION_STARTED:
      type = InterstitialType::IN_SESSION;
      break;
    case SharedSession::SESSION_PERMANENTLY_STOPPED:
      type = InterstitialType::ENCRYPTED_MEDIA_FATAL_ERROR;
      break;
    case SharedSession::SESSION_UNAVAILABLE:
    case SharedSession::SESSION_CAN_START:
    case SharedSession::SESSION_STARTING:
    case SharedSession::SESSION_STOPPING:
      break;
  }

  bool needs_update = false;
  if (image.has_value()) {
    interstitial_background_ = image.value();
    needs_update = true;
  }
  if (interstitial_natural_size_ != pipeline_metadata_.natural_size) {
    interstitial_natural_size_ = pipeline_metadata_.natural_size;
    needs_update = true;
  }
  if (interstitial_type_ != type) {
    interstitial_type_ = type;
    needs_update = true;
  }
  if (!needs_update)
    return;

  show_interstitial_cb_.Run(interstitial_background_,
                            interstitial_natural_size_, interstitial_type_);
}

void RendererController::DownloadPosterImage() {
  if (download_poster_cb_.is_null() || show_interstitial_cb_.is_null())
    return;
  DCHECK(!poster_url_.is_empty());

  const base::TimeTicks download_start_time = base::TimeTicks::Now();
  download_poster_cb_.Run(
      poster_url_,
      base::Bind(&RendererController::OnPosterImageDownloaded,
                 weak_factory_.GetWeakPtr(), poster_url_, download_start_time));
}

void RendererController::OnPosterImageDownloaded(
    const GURL& download_url,
    base::TimeTicks download_start_time,
    const SkBitmap& image) {
  DCHECK(thread_checker_.CalledOnValidThread());

  metrics_recorder_.OnPosterImageDownloaded(
      base::TimeTicks::Now() - download_start_time, !image.drawsNothing());
  if (download_url != poster_url_)
    return;  // The poster image URL has changed during the download.
  UpdateInterstitial(image);
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
  client_->ActivateViewportIntersectionMonitoring(IsRemoteSinkAvailable());
}

}  // namespace remoting
}  // namespace media
