// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_renderer.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/browser/media/cma_media_pipeline_client.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/demuxer_stream_adapter.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"

namespace chromecast {
namespace media {

namespace {
// Maximum difference between audio frame PTS and video frame PTS
// for frames read from the DemuxerStream.
const base::TimeDelta kMaxDeltaFetcher(base::TimeDelta::FromMilliseconds(2000));
}  // namespace

CastRenderer::CastRenderer(
    const scoped_refptr<CmaMediaPipelineClient> pipeline_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : pipeline_client_(pipeline_client),
      task_runner_(task_runner),
      media_task_runner_factory_(
          new BalancedMediaTaskRunnerFactory(kMaxDeltaFetcher)) {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << this;
}

CastRenderer::~CastRenderer() {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << this;
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void CastRenderer::Initialize(
    ::media::DemuxerStreamProvider* demuxer_stream_provider,
    const ::media::PipelineStatusCB& init_cb,
    const ::media::StatisticsCB& statistics_cb,
    const ::media::BufferingStateCB& buffering_state_cb,
    const base::Closure& ended_cb,
    const ::media::PipelineStatusCB& error_cb,
    const base::Closure& waiting_for_decryption_key_cb) {
  CMALOG(kLogControl) << __FUNCTION__ << ": " << this;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Create pipeline backend.
  backend_task_runner_.reset(new TaskRunnerImpl());
  // TODO(erickung): crbug.com/443956. Need to provide right LoadType.
  LoadType load_type = kLoadTypeMediaSource;
  MediaPipelineDeviceParams::MediaSyncType sync_type =
      (load_type == kLoadTypeMediaStream)
          ? MediaPipelineDeviceParams::kModeIgnorePts
          : MediaPipelineDeviceParams::kModeSyncPts;
  MediaPipelineDeviceParams params(sync_type, backend_task_runner_.get());
  scoped_ptr<MediaPipelineBackend> backend(
      pipeline_client_->CreateMediaPipelineBackend(params));

  // Create pipeline.
  MediaPipelineClient pipeline_client;
  pipeline_client.error_cb = error_cb;
  pipeline_client.buffering_state_cb = buffering_state_cb;
  pipeline_client.pipeline_backend_created_cb = base::Bind(
      &CmaMediaPipelineClient::OnMediaPipelineBackendCreated, pipeline_client_);
  pipeline_client.pipeline_backend_destroyed_cb =
      base::Bind(&CmaMediaPipelineClient::OnMediaPipelineBackendDestroyed,
                 pipeline_client_);
  pipeline_.reset(new MediaPipelineImpl);
  pipeline_->SetClient(pipeline_client);
  pipeline_->Initialize(load_type, std::move(backend));

  // Initialize audio.
  ::media::DemuxerStream* audio_stream =
      demuxer_stream_provider->GetStream(::media::DemuxerStream::AUDIO);
  if (audio_stream) {
    AvPipelineClient audio_client;
    audio_client.wait_for_key_cb = waiting_for_decryption_key_cb;
    audio_client.eos_cb =
        base::Bind(&CastRenderer::OnEos, base::Unretained(this), STREAM_AUDIO);
    audio_client.playback_error_cb = error_cb;
    audio_client.statistics_cb = statistics_cb;
    scoped_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
        task_runner_, media_task_runner_factory_, audio_stream));
    ::media::PipelineStatus status =
        pipeline_->InitializeAudio(audio_stream->audio_decoder_config(),
                                   audio_client, std::move(frame_provider));
    if (status != ::media::PIPELINE_OK) {
      init_cb.Run(status);
      return;
    }
  }

  // Initialize video.
  ::media::DemuxerStream* video_stream =
      demuxer_stream_provider->GetStream(::media::DemuxerStream::VIDEO);
  if (video_stream) {
    VideoPipelineClient video_client;
    // TODO(alokp): Set VideoPipelineClient::natural_size_changed_cb.
    video_client.av_pipeline_client.wait_for_key_cb =
        waiting_for_decryption_key_cb;
    video_client.av_pipeline_client.eos_cb =
        base::Bind(&CastRenderer::OnEos, base::Unretained(this), STREAM_VIDEO);
    video_client.av_pipeline_client.playback_error_cb = error_cb;
    video_client.av_pipeline_client.statistics_cb = statistics_cb;
    // TODO(alokp): Change MediaPipelineImpl API to accept a single config
    // after CmaRenderer is deprecated.
    std::vector<::media::VideoDecoderConfig> video_configs;
    video_configs.push_back(video_stream->video_decoder_config());
    scoped_ptr<CodedFrameProvider> frame_provider(new DemuxerStreamAdapter(
        task_runner_, media_task_runner_factory_, video_stream));
    ::media::PipelineStatus status = pipeline_->InitializeVideo(
        video_configs, video_client, std::move(frame_provider));
    if (status != ::media::PIPELINE_OK) {
      init_cb.Run(status);
      return;
    }
  }

  ended_cb_ = ended_cb;
  init_cb.Run(::media::PIPELINE_OK);
}

void CastRenderer::SetCdm(::media::CdmContext* cdm_context,
                          const ::media::CdmAttachedCB& cdm_attached_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void CastRenderer::Flush(const base::Closure& flush_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  pipeline_->Flush(flush_cb);
}

void CastRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  eos_[STREAM_AUDIO] = !pipeline_->HasAudio();
  eos_[STREAM_VIDEO] = !pipeline_->HasVideo();
  pipeline_->StartPlayingFrom(time);
}

void CastRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  pipeline_->SetPlaybackRate(playback_rate);
}

void CastRenderer::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  pipeline_->SetVolume(volume);
}

base::TimeDelta CastRenderer::GetMediaTime() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return pipeline_->GetMediaTime();
}

bool CastRenderer::HasAudio() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return pipeline_->HasAudio();
}

bool CastRenderer::HasVideo() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return pipeline_->HasVideo();
}

void CastRenderer::OnEos(Stream stream) {
  DCHECK(!eos_[stream]);
  eos_[stream] = true;
  CMALOG(kLogControl) << __FUNCTION__ << ": eos_audio=" << eos_[STREAM_AUDIO]
                      << " eos_video=" << eos_[STREAM_VIDEO];
  if (eos_[STREAM_AUDIO] && eos_[STREAM_VIDEO])
    ended_cb_.Run();
}

}  // namespace media
}  // namespace chromecast
