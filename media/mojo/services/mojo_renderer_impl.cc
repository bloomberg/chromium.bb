// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/mojo/services/mojo_demuxer_stream_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace media {

MojoRendererImpl::MojoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    mojo::ServiceProvider* audio_renderer_provider)
    : task_runner_(task_runner),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  // For now we only support audio and there must be a provider.
  DCHECK(audio_renderer_provider);
  mojo::ConnectToService(audio_renderer_provider, &remote_audio_renderer_);
  remote_audio_renderer_.set_client(this);
}

MojoRendererImpl::~MojoRendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Connection to |remote_audio_renderer_| will error-out here.
}

void MojoRendererImpl::Initialize(
    DemuxerStreamProvider* demuxer_stream_provider,
    const base::Closure& init_cb,
    const StatisticsCB& statistics_cb,
    const base::Closure& ended_cb,
    const PipelineStatusCB& error_cb,
    const BufferingStateCB& buffering_state_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream_provider);

  demuxer_stream_provider_ = demuxer_stream_provider;
  // |init_cb| can be called on other thread.
  init_cb_ = init_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  buffering_state_cb_ = buffering_state_cb;

  // Create audio and video mojo::DemuxerStream and bind its lifetime to the
  // pipe.
  DemuxerStream* const audio =
      demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* const video =
      demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);

  mojo::DemuxerStreamPtr audio_stream;
  if (audio)
    mojo::BindToProxy(new MojoDemuxerStreamImpl(audio), &audio_stream);

  mojo::DemuxerStreamPtr video_stream;
  if (video)
    mojo::BindToProxy(new MojoDemuxerStreamImpl(video), &video_stream);

  remote_audio_renderer_->Initialize(
      audio_stream.Pass(),
      video_stream.Pass(),
      BindToCurrentLoop(base::Bind(&MojoRendererImpl::OnInitialized,
                                   weak_factory_.GetWeakPtr())));
}

void MojoRendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->Flush(flush_cb);
}

void MojoRendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(lock_);
    time_ = time;
  }

  remote_audio_renderer_->StartPlayingFrom(time.InMicroseconds());
}

void MojoRendererImpl::SetPlaybackRate(float playback_rate) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererImpl::SetVolume(float volume) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_audio_renderer_->SetVolume(volume);
}

base::TimeDelta MojoRendererImpl::GetMediaTime() {
  base::AutoLock auto_lock(lock_);
  DVLOG(3) << __FUNCTION__ << ": " << time_.InMilliseconds() << " ms";
  return time_;
}

bool MojoRendererImpl::HasAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_audio_renderer_.get());  // We always bind the renderer.
  return !!demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
}

bool MojoRendererImpl::HasVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !!demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);
}

void MojoRendererImpl::SetCdm(MediaKeys* cdm) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void MojoRendererImpl::OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) {
  DVLOG(3) << __FUNCTION__ << ": " << time_usec << ", " << max_time_usec;

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MojoRendererImpl::OnTimeUpdate,
                                      weak_factory_.GetWeakPtr(),
                                      time_usec,
                                      max_time_usec));
    return;
  }

  base::AutoLock auto_lock(lock_);
  time_ = base::TimeDelta::FromMicroseconds(time_usec);
  max_time_ = base::TimeDelta::FromMicroseconds(max_time_usec);
}

void MojoRendererImpl::OnBufferingStateChange(mojo::BufferingState state) {
  DVLOG(2) << __FUNCTION__;

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MojoRendererImpl::OnBufferingStateChange,
                                      weak_factory_.GetWeakPtr(),
                                      state));
    return;
  }

  buffering_state_cb_.Run(static_cast<media::BufferingState>(state));
}

void MojoRendererImpl::OnEnded() {
  DVLOG(1) << __FUNCTION__;

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MojoRendererImpl::OnEnded, weak_factory_.GetWeakPtr()));
    return;
  }

  base::ResetAndReturn(&ended_cb_).Run();
}

void MojoRendererImpl::OnError() {
  DVLOG(1) << __FUNCTION__;

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MojoRendererImpl::OnError, weak_factory_.GetWeakPtr()));
    return;
  }

  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  if (init_cb_.is_null())  // We have initialized already.
    error_cb_.Run(PIPELINE_ERROR_DECODE);
  else
    error_cb_.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
}

void MojoRendererImpl::OnInitialized() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());

  base::ResetAndReturn(&init_cb_).Run();
}

}  // namespace media
