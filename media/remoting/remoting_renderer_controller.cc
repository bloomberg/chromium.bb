// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/remoting/remoting_cdm_context.h"

namespace media {

RemotingRendererController::RemotingRendererController(
    scoped_refptr<RemotingSourceImpl> remoting_source)
    : remoting_source_(remoting_source), weak_factory_(this) {
  remoting_source_->AddClient(this);
}

RemotingRendererController::~RemotingRendererController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics_recorder_.WillStopSession(remoting::MEDIA_ELEMENT_DESTROYED);
  remoting_source_->RemoveClient(this);
}

void RemotingRendererController::OnStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    VLOG(1) << "Remoting started successively.";
    if (remote_rendering_started_) {
      metrics_recorder_.DidStartSession();
      DCHECK(!switch_renderer_cb_.is_null());
      switch_renderer_cb_.Run();
    } else {
      remoting_source_->StopRemoting(this);
    }
  } else {
    VLOG(1) << "Failed to start remoting.";
    remote_rendering_started_ = false;
    metrics_recorder_.WillStopSession(remoting::START_RACE);
  }
}

void RemotingRendererController::OnSessionStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateFromSessionState(remoting::SINK_AVAILABLE, remoting::ROUTE_TERMINATED);
}

void RemotingRendererController::UpdateFromSessionState(
    remoting::StartTrigger start_trigger,
    remoting::StopTrigger stop_trigger) {
  VLOG(1) << "UpdateFromSessionState: " << remoting_source_->state();
  if (!sink_available_changed_cb_.is_null())
    sink_available_changed_cb_.Run(IsRemoteSinkAvailable());

  UpdateInterstitial(base::nullopt);
  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

bool RemotingRendererController::IsRemoteSinkAvailable() {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (remoting_source_->state()) {
    case SESSION_CAN_START:
    case SESSION_STARTING:
    case SESSION_STARTED:
      return true;
    case SESSION_UNAVAILABLE:
    case SESSION_STOPPING:
    case SESSION_PERMANENTLY_STOPPED:
      return false;
  }

  return false;  // To suppress compile warning.
}

void RemotingRendererController::OnEnteredFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = true;
  // See notes in OnBecameDominantVisibleContent() for why this is forced:
  is_dominant_content_ = true;
  UpdateAndMaybeSwitch(remoting::ENTERED_FULLSCREEN,
                       remoting::UNKNOWN_STOP_TRIGGER);
}

void RemotingRendererController::OnExitedFullscreen() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_fullscreen_ = false;
  // See notes in OnBecameDominantVisibleContent() for why this is forced:
  is_dominant_content_ = false;
  UpdateAndMaybeSwitch(remoting::UNKNOWN_START_TRIGGER,
                       remoting::EXITED_FULLSCREEN);
}

void RemotingRendererController::OnBecameDominantVisibleContent(
    bool is_dominant) {
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
  UpdateAndMaybeSwitch(remoting::BECAME_DOMINANT_CONTENT,
                       remoting::BECAME_AUXILIARY_CONTENT);
}

void RemotingRendererController::OnSetCdm(CdmContext* cdm_context) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto* remoting_cdm_context = RemotingCdmContext::From(cdm_context);
  if (!remoting_cdm_context)
    return;

  remoting_source_->RemoveClient(this);
  remoting_source_ = remoting_cdm_context->GetRemotingSource();
  remoting_source_->AddClient(this);
  UpdateFromSessionState(remoting::CDM_READY, remoting::DECRYPTION_ERROR);
}

void RemotingRendererController::OnRemotePlaybackDisabled(bool disabled) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_remote_playback_disabled_ = disabled;
  metrics_recorder_.OnRemotePlaybackDisabled(disabled);
  UpdateAndMaybeSwitch(remoting::ENABLED_BY_PAGE, remoting::DISABLED_BY_PAGE);
}

void RemotingRendererController::OnSetPoster(const GURL& poster_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (poster_url != poster_url_) {
    poster_url_ = poster_url;
    if (poster_url_.is_empty())
      UpdateInterstitial(SkBitmap());
    else
      DownloadPosterImage();
  }
}

void RemotingRendererController::SetSwitchRendererCallback(
    const base::Closure& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!cb.is_null());

  switch_renderer_cb_ = cb;
  // Note: Not calling UpdateAndMaybeSwitch() here since this method should be
  // called during the initialization phase of this RemotingRendererController;
  // and definitely before a whole lot of other things that are needed to
  // trigger a switch.
}

void RemotingRendererController::SetRemoteSinkAvailableChangedCallback(
    const base::Callback<void(bool)>& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  sink_available_changed_cb_ = cb;
  if (!sink_available_changed_cb_.is_null())
    sink_available_changed_cb_.Run(IsRemoteSinkAvailable());
}

base::WeakPtr<remoting::RpcBroker> RemotingRendererController::GetRpcBroker()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return remoting_source_->GetRpcBroker()->GetWeakPtr();
}

void RemotingRendererController::StartDataPipe(
    std::unique_ptr<mojo::DataPipe> audio_data_pipe,
    std::unique_ptr<mojo::DataPipe> video_data_pipe,
    const RemotingSourceImpl::DataPipeStartCallback& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  remoting_source_->StartDataPipe(std::move(audio_data_pipe),
                                  std::move(video_data_pipe), done_callback);
}

void RemotingRendererController::OnMetadataChanged(
    const PipelineMetadata& metadata) {
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

  remoting::StartTrigger start_trigger = remoting::UNKNOWN_START_TRIGGER;
  if (!was_audio_codec_supported && is_audio_codec_supported)
    start_trigger = remoting::SUPPORTED_AUDIO_CODEC;
  if (!was_video_codec_supported && is_video_codec_supported) {
    start_trigger = start_trigger == remoting::SUPPORTED_AUDIO_CODEC
                        ? remoting::SUPPORTED_AUDIO_AND_VIDEO_CODECS
                        : remoting::SUPPORTED_VIDEO_CODEC;
  }
  remoting::StopTrigger stop_trigger = remoting::UNKNOWN_STOP_TRIGGER;
  if (was_audio_codec_supported && !is_audio_codec_supported)
    stop_trigger = remoting::UNSUPPORTED_AUDIO_CODEC;
  if (was_video_codec_supported && !is_video_codec_supported) {
    stop_trigger = stop_trigger == remoting::UNSUPPORTED_AUDIO_CODEC
                       ? remoting::UNSUPPORTED_AUDIO_AND_VIDEO_CODECS
                       : remoting::UNSUPPORTED_VIDEO_CODEC;
  }
  UpdateAndMaybeSwitch(start_trigger, stop_trigger);
}

bool RemotingRendererController::IsVideoCodecSupported() {
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

bool RemotingRendererController::IsAudioCodecSupported() {
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

void RemotingRendererController::OnPlaying() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = false;
  UpdateAndMaybeSwitch(remoting::PLAY_COMMAND, remoting::UNKNOWN_STOP_TRIGGER);
}

void RemotingRendererController::OnPaused() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = true;
}

bool RemotingRendererController::ShouldBeRemoting() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (switch_renderer_cb_.is_null()) {
    DCHECK(!remote_rendering_started_);
    return false;  // No way to switch to a RemotingRenderImpl.
  }

  const RemotingSessionState state = remoting_source_->state();
  if (is_encrypted_) {
    // Due to technical limitations when playing encrypted content, once a
    // remoting session has been started, always return true here to indicate
    // that the RemotingRendererImpl should be used. In the stopped states,
    // RemotingRendererImpl will display an interstitial to notify the user that
    // local rendering cannot be resumed.
    //
    // TODO(miu): Revisit this once more of the encrypted-remoting impl is
    // in-place. For example, this will prevent metrics from recording session
    // stop reasons.
    return state == RemotingSessionState::SESSION_STARTED ||
           state == RemotingSessionState::SESSION_STOPPING ||
           state == RemotingSessionState::SESSION_PERMANENTLY_STOPPED;
  }

  if (encountered_renderer_fatal_error_)
    return false;

  switch (state) {
    case SESSION_UNAVAILABLE:
      return false;  // Cannot remote media without a remote sink.
    case SESSION_CAN_START:
    case SESSION_STARTING:
    case SESSION_STARTED:
      break;  // Media remoting is possible, assuming other requirments are met.
    case SESSION_STOPPING:
    case SESSION_PERMANENTLY_STOPPED:
      return false;  // Use local rendering after stopping remoting.
  }

  switch (remoting_source_->sink_capabilities()) {
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

void RemotingRendererController::UpdateAndMaybeSwitch(
    remoting::StartTrigger start_trigger,
    remoting::StopTrigger stop_trigger) {
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

  if (remote_rendering_started_) {
    DCHECK(!switch_renderer_cb_.is_null());
    if (remoting_source_->state() ==
        RemotingSessionState::SESSION_PERMANENTLY_STOPPED) {
      switch_renderer_cb_.Run();
      return;
    }
    DCHECK_NE(start_trigger, remoting::UNKNOWN_START_TRIGGER);
    metrics_recorder_.WillStartSession(start_trigger);
    // |switch_renderer_cb_.Run()| will be called after remoting is started
    // successfully.
    remoting_source_->StartRemoting(this);
  } else {
    // For encrypted content, it's only valid to switch to remoting renderer,
    // and never back to the local renderer. The RemotingCdmController will
    // force-stop the session when remoting has ended; so no need to call
    // StopRemoting() from here.
    DCHECK(!is_encrypted_);
    DCHECK_NE(stop_trigger, remoting::UNKNOWN_STOP_TRIGGER);
    metrics_recorder_.WillStopSession(stop_trigger);
    switch_renderer_cb_.Run();
    remoting_source_->StopRemoting(this);
  }
}

void RemotingRendererController::SetShowInterstitialCallback(
    const ShowInterstitialCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  show_interstitial_cb_ = cb;
  UpdateInterstitial(SkBitmap());
  if (!poster_url_.is_empty())
    DownloadPosterImage();
}

void RemotingRendererController::SetDownloadPosterCallback(
    const DownloadPosterCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(download_poster_cb_.is_null());
  download_poster_cb_ = cb;
  if (!poster_url_.is_empty())
    DownloadPosterImage();
}

void RemotingRendererController::UpdateInterstitial(
    const base::Optional<SkBitmap>& image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (show_interstitial_cb_.is_null() ||
      pipeline_metadata_.natural_size.IsEmpty())
    return;

  RemotingInterstitialType type = RemotingInterstitialType::BETWEEN_SESSIONS;
  switch (remoting_source_->state()) {
    case SESSION_STARTED:
      type = RemotingInterstitialType::IN_SESSION;
      break;
    case SESSION_PERMANENTLY_STOPPED:
      type = RemotingInterstitialType::ENCRYPTED_MEDIA_FATAL_ERROR;
      break;
    case SESSION_UNAVAILABLE:
    case SESSION_CAN_START:
    case SESSION_STARTING:
    case SESSION_STOPPING:
      break;
  }

  show_interstitial_cb_.Run(image, pipeline_metadata_.natural_size, type);
}

void RemotingRendererController::DownloadPosterImage() {
  if (download_poster_cb_.is_null() || show_interstitial_cb_.is_null())
    return;
  DCHECK(!poster_url_.is_empty());

  const base::TimeTicks download_start_time = base::TimeTicks::Now();
  download_poster_cb_.Run(
      poster_url_,
      base::Bind(&RemotingRendererController::OnPosterImageDownloaded,
                 weak_factory_.GetWeakPtr(), poster_url_, download_start_time));
}

void RemotingRendererController::OnPosterImageDownloaded(
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

void RemotingRendererController::OnRendererFatalError(
    remoting::StopTrigger stop_trigger) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not act on errors caused by things like Mojo pipes being closed during
  // shutdown.
  if (!remote_rendering_started_)
    return;

  encountered_renderer_fatal_error_ = true;
  UpdateAndMaybeSwitch(remoting::UNKNOWN_START_TRIGGER, stop_trigger);
}

}  // namespace media
