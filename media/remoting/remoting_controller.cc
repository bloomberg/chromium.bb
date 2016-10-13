// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace media {

RemotingController::RemotingController(
    mojom::RemotingSourceRequest source_request,
    mojom::RemoterPtr remoter)
    : binding_(this, std::move(source_request)),
      remoter_(std::move(remoter)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

RemotingController::~RemotingController() {}

void RemotingController::OnSinkAvailable() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_sink_available_ = true;
  UpdateAndMaybeSwitch();
}

void RemotingController::OnSinkGone() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_sink_available_ = false;
  UpdateAndMaybeSwitch();
}

void RemotingController::OnStarted() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  VLOG(1) << "Remoting started successively.";
  if (is_remoting_)
    switch_renderer_cb_.Run();
  else
    remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
}

void RemotingController::OnStartFailed(mojom::RemotingStartFailReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  VLOG(1) << "Failed to start remoting:" << reason;
  is_remoting_ = false;
}

void RemotingController::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(xjz): Merge with Eric's CL to handle the RPC messages here.
  NOTIMPLEMENTED();
}

void RemotingController::OnStopped(mojom::RemotingStopReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  VLOG(1) << "Remoting stopped: " << reason;
  is_remoting_ = false;
}

void RemotingController::OnEnteredFullscreen() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_fullscreen_ = true;
  UpdateAndMaybeSwitch();
}

void RemotingController::OnExitedFullscreen() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  is_fullscreen_ = false;
  UpdateAndMaybeSwitch();
}

void RemotingController::OnSetCdm(CdmContext* cdm_context) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(xjz): Not implemented. Will add in up-coming change.
  NOTIMPLEMENTED();
}

void RemotingController::SetSwitchRendererCallback(
    const SwitchRendererCallback& cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!cb.is_null());

  switch_renderer_cb_ = cb;
}

void RemotingController::OnMetadataChanged(const PipelineMetadata& metadata) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_video_ = metadata.has_video;
  has_audio_ = metadata.has_audio;
  if (!has_video_ && !has_audio_)
    return;

  // On Android, when using the MediaPlayerRenderer, |has_video_| and
  // |has_audio_| will be true, but the respective configs will be empty.
  // We cannot make any assumptions on the validity of configs.
  if (has_video_) {
    video_decoder_config_ = metadata.video_decoder_config;
    is_encrypted_ |= video_decoder_config_.is_encrypted();
  }
  if (has_audio_) {
    audio_decoder_config_ = metadata.audio_decoder_config;
    is_encrypted_ |= audio_decoder_config_.is_encrypted();
  }
  UpdateAndMaybeSwitch();
}

bool RemotingController::IsVideoCodecSupported() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(has_video_);

  switch (video_decoder_config_.codec()) {
    case VideoCodec::kCodecH264:
    case VideoCodec::kCodecVP8:
      return true;
    default:
      VLOG(2) << "Remoting does not support video codec: "
              << video_decoder_config_.codec();
      return false;
  }
}

bool RemotingController::IsAudioCodecSupported() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(has_audio_);

  switch (audio_decoder_config_.codec()) {
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
              << audio_decoder_config_.codec();
      return false;
  }
}

bool RemotingController::ShouldBeRemoting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(xjz): The control logic for EME will be added in a later CL.
  if (is_encrypted_)
    return false;

  if (!is_sink_available_)
    return false;
  if (!is_fullscreen_)
    return false;
  if (has_video_ && !IsVideoCodecSupported())
    return false;
  if (has_audio_ && !IsAudioCodecSupported())
    return false;
  return true;
}

void RemotingController::UpdateAndMaybeSwitch() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(xjz): The switching logic for encrypted content will be added in a
  // later CL.

  // Demuxer is not initialized yet.
  if (!has_audio_ && !has_video_)
    return;

  DCHECK(!switch_renderer_cb_.is_null());

  bool should_be_remoting = ShouldBeRemoting();
  if (is_remoting_ == should_be_remoting)
    return;

  // Switch between local and remoting.
  is_remoting_ = should_be_remoting;
  if (is_remoting_) {
    // |swithc_renderer_cb_.Run()| will be called after remoting is started
    // successfully.
    remoter_->Start();
  } else {
    switch_renderer_cb_.Run();
    remoter_->Stop(mojom::RemotingStopReason::LOCAL_PLAYBACK);
  }
}

}  // namespace media
