// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/mojo/services/mojo_demuxer_stream_impl.h"

namespace media {

MojoRendererImpl::MojoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    interfaces::RendererPtr remote_renderer)
    : task_runner_(task_runner),
      remote_renderer_(std::move(remote_renderer)),
      binding_(this) {
  DVLOG(1) << __FUNCTION__;
}

MojoRendererImpl::~MojoRendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
}

// TODO(xhwang): Support |waiting_for_decryption_key_cb| and |statictics_cb|.
// See http://crbug.com/585287
void MojoRendererImpl::Initialize(
    DemuxerStreamProvider* demuxer_stream_provider,
    const PipelineStatusCB& init_cb,
    const StatisticsCB& /* statistics_cb */,
    const BufferingStateCB& buffering_state_cb,
    const base::Closure& ended_cb,
    const PipelineStatusCB& error_cb,
    const base::Closure& /* waiting_for_decryption_key_cb */) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream_provider);

  // If connection error has happened, fail immediately.
  if (remote_renderer_.encountered_error()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(init_cb, PIPELINE_ERROR_INITIALIZATION_FAILED));
    return;
  }

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the error handler can't be invoked once
  // |remote_renderer_| is destroyed.
  remote_renderer_.set_connection_error_handler(
      base::Bind(&MojoRendererImpl::OnConnectionError, base::Unretained(this)));

  demuxer_stream_provider_ = demuxer_stream_provider;
  init_cb_ = init_cb;
  buffering_state_cb_ = buffering_state_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;

  // Create audio and video interfaces::DemuxerStream and bind its lifetime to
  // the pipe.
  DemuxerStream* const audio =
      demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* const video =
      demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);

  interfaces::DemuxerStreamPtr audio_stream;
  if (audio)
    new MojoDemuxerStreamImpl(audio, GetProxy(&audio_stream));

  interfaces::DemuxerStreamPtr video_stream;
  if (video)
    new MojoDemuxerStreamImpl(video, GetProxy(&video_stream));

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  remote_renderer_->Initialize(
      binding_.CreateInterfacePtrAndBind(), std::move(audio_stream),
      std::move(video_stream),
      base::Bind(&MojoRendererImpl::OnInitialized, base::Unretained(this)));
}

void MojoRendererImpl::SetCdm(CdmContext* cdm_context,
                              const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  int32_t cdm_id = cdm_context->GetCdmId();
  if (cdm_id == CdmContext::kInvalidCdmId) {
    DVLOG(2) << "MojoRendererImpl only works with remote CDMs but the CDM ID "
                "is invalid.";
    cdm_attached_cb.Run(false);
    return;
  }

  remote_renderer_->SetCdm(cdm_id, cdm_attached_cb);
}

void MojoRendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_renderer_->Flush(flush_cb);
}

void MojoRendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(lock_);
    time_ = time;
  }

  remote_renderer_->StartPlayingFrom(time.InMicroseconds());
}

void MojoRendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererImpl::SetVolume(float volume) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_renderer_->SetVolume(volume);
}

base::TimeDelta MojoRendererImpl::GetMediaTime() {
  base::AutoLock auto_lock(lock_);
  DVLOG(3) << __FUNCTION__ << ": " << time_.InMilliseconds() << " ms";
  return time_;
}

bool MojoRendererImpl::HasAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.get());  // We always bind the renderer.
  return !!demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
}

bool MojoRendererImpl::HasVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !!demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);
}

void MojoRendererImpl::OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) {
  DVLOG(3) << __FUNCTION__ << ": " << time_usec << ", " << max_time_usec;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  time_ = base::TimeDelta::FromMicroseconds(time_usec);
}

void MojoRendererImpl::OnBufferingStateChange(
    interfaces::BufferingState state) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  buffering_state_cb_.Run(static_cast<media::BufferingState>(state));
}

void MojoRendererImpl::OnEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  ended_cb_.Run();
}

void MojoRendererImpl::OnError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null());

  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  error_cb_.Run(PIPELINE_ERROR_DECODE);
}

void MojoRendererImpl::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!init_cb_.is_null()) {
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  error_cb_.Run(PIPELINE_ERROR_DECODE);
}

void MojoRendererImpl::OnInitialized(bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());

  base::ResetAndReturn(&init_cb_)
      .Run(success ? PIPELINE_OK : PIPELINE_ERROR_INITIALIZATION_FAILED);
}

}  // namespace media
