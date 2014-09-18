// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/mojo/services/mojo_demuxer_stream_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace media {

MojoRendererImpl::MojoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    DemuxerStreamProvider* demuxer_stream_provider,
    mojo::ServiceProvider* audio_renderer_provider)
    : task_runner_(task_runner),
      demuxer_stream_provider_(demuxer_stream_provider),
      weak_factory_(this) {
  // For now we only support audio and there must be a provider.
  DCHECK(audio_renderer_provider);
  mojo::ConnectToService(audio_renderer_provider, &remote_audio_renderer_);
  remote_audio_renderer_.set_client(this);
}

MojoRendererImpl::~MojoRendererImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Connection to |remote_audio_renderer_| will error-out here.
}

void MojoRendererImpl::Initialize(const base::Closure& init_cb,
                                  const StatisticsCB& statistics_cb,
                                  const base::Closure& ended_cb,
                                  const PipelineStatusCB& error_cb,
                                  const BufferingStateCB& buffering_state_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  init_cb_ = init_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  buffering_state_cb_ = buffering_state_cb;

  // Create a mojo::DemuxerStream and bind its lifetime to the pipe.
  mojo::DemuxerStreamPtr demuxer_stream;
  mojo::BindToProxy(
      new MojoDemuxerStreamImpl(
          demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO)),
      &demuxer_stream);
  remote_audio_renderer_->Initialize(demuxer_stream.Pass(), init_cb);
}

void MojoRendererImpl::Flush(const base::Closure& flush_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->Flush(flush_cb);
}

void MojoRendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->StartPlayingFrom(time.InMicroseconds());
}

void MojoRendererImpl::SetPlaybackRate(float playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererImpl::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->SetVolume(volume);
}

base::TimeDelta MojoRendererImpl::GetMediaTime() {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

bool MojoRendererImpl::HasAudio() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_audio_renderer_.get());  // We always bind the renderer.
  return true;
}

bool MojoRendererImpl::HasVideo() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return false;
}

void MojoRendererImpl::SetCdm(MediaKeys* cdm) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void MojoRendererImpl::OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void MojoRendererImpl::OnBufferingStateChange(mojo::BufferingState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  buffering_state_cb_.Run(static_cast<media::BufferingState>(state));
}

void MojoRendererImpl::OnEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ended_cb_.Run();
}

void MojoRendererImpl::OnError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  if (init_cb_.is_null())  // We have initialized already.
    error_cb_.Run(PIPELINE_ERROR_DECODE);
  else
    error_cb_.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
}

void MojoRendererImpl::OnInitialized() {
  DCHECK(!init_cb_.is_null());
  base::ResetAndReturn(&init_cb_).Run();
}

}  // namespace media
