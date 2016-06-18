// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/renderers/video_overlay_factory.h"

namespace media {

MojoRendererImpl::MojoRendererImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
    VideoRendererSink* video_renderer_sink,
    mojom::RendererPtr remote_renderer)
    : task_runner_(task_runner),
      video_overlay_factory_(std::move(video_overlay_factory)),
      video_renderer_sink_(video_renderer_sink),
      remote_renderer_info_(remote_renderer.PassInterface()),
      binding_(this) {
  DVLOG(1) << __FUNCTION__;
}

MojoRendererImpl::~MojoRendererImpl() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  CancelPendingCallbacks();
}

void MojoRendererImpl::Initialize(
    DemuxerStreamProvider* demuxer_stream_provider,
    media::RendererClient* client,
    const PipelineStatusCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(demuxer_stream_provider);

  if (encountered_error_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(init_cb, PIPELINE_ERROR_INITIALIZATION_FAILED));
    return;
  }

  demuxer_stream_provider_ = demuxer_stream_provider;
  init_cb_ = init_cb;

  // Create audio and video mojom::DemuxerStream and bind its lifetime to
  // the pipe.
  DemuxerStream* const audio =
      demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* const video =
      demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);

  mojom::DemuxerStreamPtr audio_stream;
  if (audio)
    new MojoDemuxerStreamImpl(audio, GetProxy(&audio_stream));

  mojom::DemuxerStreamPtr video_stream;
  if (video)
    new MojoDemuxerStreamImpl(video, GetProxy(&video_stream));

  BindRemoteRendererIfNeeded();

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  remote_renderer_->Initialize(binding_.CreateInterfacePtrAndBind(),
                               std::move(audio_stream), std::move(video_stream),
                               base::Bind(&MojoRendererImpl::OnInitialized,
                                          base::Unretained(this), client));
}

void MojoRendererImpl::SetCdm(CdmContext* cdm_context,
                              const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);
  DCHECK(!cdm_attached_cb.is_null());
  DCHECK(cdm_attached_cb_.is_null());

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(cdm_attached_cb, false));
    return;
  }

  int32_t cdm_id = cdm_context->GetCdmId();
  if (cdm_id == CdmContext::kInvalidCdmId) {
    DVLOG(2) << "MojoRendererImpl only works with remote CDMs but the CDM ID "
                "is invalid.";
    task_runner_->PostTask(FROM_HERE, base::Bind(cdm_attached_cb, false));
    return;
  }

  BindRemoteRendererIfNeeded();

  cdm_attached_cb_ = cdm_attached_cb;
  remote_renderer_->SetCdm(cdm_id, base::Bind(&MojoRendererImpl::OnCdmAttached,
                                              base::Unretained(this)));
}

void MojoRendererImpl::Flush(const base::Closure& flush_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());
  DCHECK(!flush_cb.is_null());
  DCHECK(flush_cb_.is_null());

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE, flush_cb);
    return;
  }

  flush_cb_ = flush_cb;
  remote_renderer_->Flush(
      base::Bind(&MojoRendererImpl::OnFlushed, base::Unretained(this)));
}

void MojoRendererImpl::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  {
    base::AutoLock auto_lock(lock_);
    time_ = time;
  }

  remote_renderer_->StartPlayingFrom(time.InMicroseconds());
}

void MojoRendererImpl::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  remote_renderer_->SetPlaybackRate(playback_rate);
}

void MojoRendererImpl::SetVolume(float volume) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

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
  DCHECK(remote_renderer_.is_bound());

  return !!demuxer_stream_provider_->GetStream(DemuxerStream::AUDIO);
}

bool MojoRendererImpl::HasVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  return !!demuxer_stream_provider_->GetStream(DemuxerStream::VIDEO);
}

void MojoRendererImpl::OnTimeUpdate(int64_t time_usec, int64_t max_time_usec) {
  DVLOG(3) << __FUNCTION__ << ": " << time_usec << ", " << max_time_usec;
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  time_ = base::TimeDelta::FromMicroseconds(time_usec);
}

void MojoRendererImpl::OnBufferingStateChange(mojom::BufferingState state) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(static_cast<media::BufferingState>(state));
}

void MojoRendererImpl::OnEnded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void MojoRendererImpl::OnError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null());

  encountered_error_ = true;

  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRendererImpl::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __FUNCTION__ << ": " << size.ToString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  video_renderer_sink_->PaintSingleFrame(
      video_overlay_factory_->CreateFrame(size));
  client_->OnVideoNaturalSizeChange(size);
}

void MojoRendererImpl::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __FUNCTION__ << ": " << opaque;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoOpacityChange(opaque);
}

void MojoRendererImpl::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  encountered_error_ = true;
  CancelPendingCallbacks();

  if (client_)
    client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRendererImpl::BindRemoteRendererIfNeeded() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_renderer_| has already been bound, do nothing.
  // Note that after Bind() is called, |remote_renderer_| is always bound even
  // after connection error.
  if (remote_renderer_.is_bound())
    return;

  // Bind |remote_renderer_| to the |task_runner_|.
  remote_renderer_.Bind(std::move(remote_renderer_info_));

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the error handler can't be invoked once
  // |remote_renderer_| is destroyed.
  remote_renderer_.set_connection_error_handler(
      base::Bind(&MojoRendererImpl::OnConnectionError, base::Unretained(this)));
}

void MojoRendererImpl::OnInitialized(media::RendererClient* client,
                                     bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_.is_null());

  // Only set |client_| after initialization succeeded. No client methods should
  // be called before this.
  if (success)
    client_ = client;

  base::ResetAndReturn(&init_cb_).Run(
      success ? PIPELINE_OK : PIPELINE_ERROR_INITIALIZATION_FAILED);
}

void MojoRendererImpl::OnFlushed() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!flush_cb_.is_null());

  base::ResetAndReturn(&flush_cb_).Run();
}

void MojoRendererImpl::OnCdmAttached(bool success) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!cdm_attached_cb_.is_null());

  base::ResetAndReturn(&cdm_attached_cb_).Run(success);
}

void MojoRendererImpl::CancelPendingCallbacks() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);

  if (!flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run();

  if (!cdm_attached_cb_.is_null())
    base::ResetAndReturn(&cdm_attached_cb_).Run(false);
}

}  // namespace media
