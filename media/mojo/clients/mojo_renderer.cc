// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

MojoRenderer::MojoRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
    VideoRendererSink* video_renderer_sink,
    mojo::PendingRemote<mojom::Renderer> remote_renderer)
    : task_runner_(task_runner),
      video_overlay_factory_(std::move(video_overlay_factory)),
      video_renderer_sink_(video_renderer_sink),
      remote_renderer_pending_remote_(std::move(remote_renderer)),
      client_binding_(this),
      media_time_interpolator_(base::DefaultTickClock::GetInstance()) {
  DVLOG(1) << __func__;
}

MojoRenderer::~MojoRenderer() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  CancelPendingCallbacks();
}

void MojoRenderer::Initialize(MediaResource* media_resource,
                              media::RendererClient* client,
                              PipelineStatusCallback init_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(media_resource);

  if (encountered_error_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  PIPELINE_ERROR_INITIALIZATION_FAILED));
    return;
  }

  media_resource_ = media_resource;
  init_cb_ = std::move(init_cb);

  switch (media_resource_->GetType()) {
    case MediaResource::Type::STREAM:
      InitializeRendererFromStreams(client);
      break;
    case MediaResource::Type::URL:
      InitializeRendererFromUrl(client);
      break;
  }
}

void MojoRenderer::InitializeRendererFromStreams(
    media::RendererClient* client) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Create mojom::DemuxerStream for each demuxer stream and bind its lifetime
  // to the pipe.
  std::vector<DemuxerStream*> streams = media_resource_->GetAllStreams();
  std::vector<mojo::PendingRemote<mojom::DemuxerStream>> stream_proxies;

  for (auto* stream : streams) {
    mojo::PendingRemote<mojom::DemuxerStream> stream_proxy;
    auto mojo_stream = std::make_unique<MojoDemuxerStreamImpl>(
        stream, stream_proxy.InitWithNewPipeAndPassReceiver());

    // Using base::Unretained(this) is safe because |this| owns |mojo_stream|,
    // and the error handler can't be invoked once |mojo_stream| is destroyed.
    mojo_stream->set_disconnect_handler(
        base::Bind(&MojoRenderer::OnDemuxerStreamConnectionError,
                   base::Unretained(this), mojo_stream.get()));

    streams_.push_back(std::move(mojo_stream));
    stream_proxies.push_back(std::move(stream_proxy));
  }

  BindRemoteRendererIfNeeded();

  mojom::RendererClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr_info));

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  remote_renderer_->Initialize(
      std::move(client_ptr_info), std::move(stream_proxies), nullptr,
      base::Bind(&MojoRenderer::OnInitialized, base::Unretained(this), client));
}

void MojoRenderer::InitializeRendererFromUrl(media::RendererClient* client) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  BindRemoteRendererIfNeeded();

  mojom::RendererClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr_info));

  const MediaUrlParams& url_params = media_resource_->GetMediaUrlParams();

  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the callback won't be dispatched if
  // |remote_renderer_| is destroyed.
  mojom::MediaUrlParamsPtr media_url_params = mojom::MediaUrlParams::New(
      url_params.media_url, url_params.site_for_cookies,
      url_params.top_frame_origin, url_params.allow_credentials,
      url_params.is_hls);
  remote_renderer_->Initialize(
      std::move(client_ptr_info), base::nullopt, std::move(media_url_params),
      base::Bind(&MojoRenderer::OnInitialized, base::Unretained(this), client));
}

void MojoRenderer::SetCdm(CdmContext* cdm_context,
                          CdmAttachedCB cdm_attached_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);
  DCHECK(cdm_attached_cb);
  DCHECK(!cdm_attached_cb_);

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(cdm_attached_cb), false));
    return;
  }

  int32_t cdm_id = cdm_context->GetCdmId();
  if (cdm_id == CdmContext::kInvalidCdmId) {
    DVLOG(2) << "MojoRenderer only works with remote CDMs but the CDM ID "
                "is invalid.";
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(cdm_attached_cb), false));
    return;
  }

  BindRemoteRendererIfNeeded();

  cdm_attached_cb_ = std::move(cdm_attached_cb);
  remote_renderer_->SetCdm(cdm_id, base::BindOnce(&MojoRenderer::OnCdmAttached,
                                                  base::Unretained(this)));
}

void MojoRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());
  DCHECK(flush_cb);
  DCHECK(!flush_cb_);

  if (encountered_error_) {
    task_runner_->PostTask(FROM_HERE, std::move(flush_cb));
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (media_time_interpolator_.interpolating())
      media_time_interpolator_.StopInterpolating();
  }

  flush_cb_ = std::move(flush_cb);
  remote_renderer_->Flush(
      base::Bind(&MojoRenderer::OnFlushed, base::Unretained(this)));
}

void MojoRenderer::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(2) << __func__ << "(" << time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetBounds(time, time, base::TimeTicks::Now());
    media_time_interpolator_.StartInterpolating();
  }

  remote_renderer_->StartPlayingFrom(time);
}

void MojoRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG(2) << __func__ << "(" << playback_rate << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  remote_renderer_->SetPlaybackRate(playback_rate);

  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetPlaybackRate(playback_rate);
  }
}

void MojoRenderer::SetVolume(float volume) {
  DVLOG(2) << __func__ << "(" << volume << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(remote_renderer_.is_bound());

  remote_renderer_->SetVolume(volume);
}

base::TimeDelta MojoRenderer::GetMediaTime() {
  base::AutoLock auto_lock(lock_);
  return media_time_interpolator_.GetInterpolatedTime();
}

void MojoRenderer::OnTimeUpdate(base::TimeDelta time,
                                base::TimeDelta max_time,
                                base::TimeTicks capture_time) {
  DVLOG(4) << __func__ << "(" << time << ", " << max_time << ", "
           << capture_time << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  media_time_interpolator_.SetBounds(time, max_time, capture_time);
}

void MojoRenderer::OnBufferingStateChange(BufferingState state,
                                          BufferingStateChangeReason reason) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnBufferingStateChange(state, reason);
}

void MojoRenderer::OnEnded() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnEnded();
}

void MojoRenderer::OnError() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!init_cb_);

  encountered_error_ = true;

  // TODO(tim): Should we plumb error code from remote renderer?
  // http://crbug.com/410451.
  client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRenderer::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(2) << __func__ << ": " << size.ToString();
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (video_overlay_factory_) {
    video_renderer_sink_->PaintSingleFrame(
        video_overlay_factory_->CreateFrame(size));
  }
  client_->OnVideoNaturalSizeChange(size);
}

void MojoRenderer::OnVideoOpacityChange(bool opaque) {
  DVLOG(2) << __func__ << ": " << opaque;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoOpacityChange(opaque);
}

void MojoRenderer::OnAudioConfigChange(const AudioDecoderConfig& config) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnAudioConfigChange(config);
}

void MojoRenderer::OnVideoConfigChange(const VideoDecoderConfig& config) {
  DVLOG(2) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnVideoConfigChange(config);
}

void MojoRenderer::OnStatisticsUpdate(const PipelineStatistics& stats) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!client_) {
    pending_stats_ = stats;
    return;
  }
  client_->OnStatisticsUpdate(stats);
}

void MojoRenderer::OnWaiting(WaitingReason reason) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->OnWaiting(reason);
}

void MojoRenderer::OnConnectionError() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  encountered_error_ = true;
  CancelPendingCallbacks();

  if (client_)
    client_->OnError(PIPELINE_ERROR_DECODE);
}

void MojoRenderer::OnDemuxerStreamConnectionError(
    MojoDemuxerStreamImpl* stream) {
  DVLOG(1) << __func__ << ": stream=" << stream;
  DCHECK(task_runner_->BelongsToCurrentThread());

  for (auto& s : streams_) {
    if (s.get() == stream) {
      s.reset();
      return;
    }
  }
  NOTREACHED() << "Unrecognized demuxer stream=" << stream;
}

void MojoRenderer::BindRemoteRendererIfNeeded() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_renderer_| has already been bound, do nothing.
  // Note that after Bind() is called, |remote_renderer_| is always bound even
  // after connection error.
  if (remote_renderer_.is_bound())
    return;

  // Bind |remote_renderer_| to the |remote_renderer_pending_remote_|.
  remote_renderer_.Bind(std::move(remote_renderer_pending_remote_));

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns
  // |remote_renderer_|, and the error handler can't be invoked once
  // |remote_renderer_| is destroyed.
  remote_renderer_.set_disconnect_handler(
      base::Bind(&MojoRenderer::OnConnectionError, base::Unretained(this)));
}

void MojoRenderer::OnInitialized(media::RendererClient* client, bool success) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_);

  // Only set |client_| after initialization succeeded. No client methods should
  // be called before this.
  if (success)
    client_ = client;

  std::move(init_cb_).Run(success ? PIPELINE_OK
                                  : PIPELINE_ERROR_INITIALIZATION_FAILED);

  if (client_ && pending_stats_.has_value())
    client_->OnStatisticsUpdate(pending_stats_.value());
  pending_stats_.reset();
}

void MojoRenderer::OnFlushed() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(flush_cb_);

  std::move(flush_cb_).Run();
}

void MojoRenderer::OnCdmAttached(bool success) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_attached_cb_);

  std::move(cdm_attached_cb_).Run(success);
}

void MojoRenderer::CancelPendingCallbacks() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (init_cb_)
    std::move(init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);

  if (flush_cb_)
    std::move(flush_cb_).Run();

  if (cdm_attached_cb_)
    std::move(cdm_attached_cb_).Run(false);
}

}  // namespace media
