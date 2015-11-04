// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"

#include "base/bind.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxVideoFrameSize = 1024 * 1024;
}

VideoPipelineImpl::VideoPipelineImpl(
    MediaPipelineBackend::VideoDecoder* video_decoder,
    const VideoPipelineClient& client)
    : video_decoder_(video_decoder),
      video_client_(client),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  av_pipeline_impl_.reset(new AvPipelineImpl(
      video_decoder_,
      base::Bind(&VideoPipelineImpl::OnUpdateConfig, base::Unretained(this))));
}

VideoPipelineImpl::~VideoPipelineImpl() {
}

void VideoPipelineImpl::SetCodedFrameProvider(
    scoped_ptr<CodedFrameProvider> frame_provider) {
  av_pipeline_impl_->SetCodedFrameProvider(
      frame_provider.Pass(), kAppVideoBufferSize, kMaxVideoFrameSize);
}

bool VideoPipelineImpl::StartPlayingFrom(
    base::TimeDelta time,
    const scoped_refptr<BufferingState>& buffering_state) {
  CMALOG(kLogControl) << __FUNCTION__ << " t0=" << time.InMilliseconds();

  // Reset the pipeline statistics.
  previous_stats_ = ::media::PipelineStatistics();

  // Start playing.
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError)
    return false;
  DCHECK_EQ(av_pipeline_impl_->GetState(), AvPipelineImpl::kFlushed);

  if (!av_pipeline_impl_->StartPlayingFrom(time, buffering_state)) {
    av_pipeline_impl_->TransitionToState(AvPipelineImpl::kError);
    return false;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kPlaying);

  return true;
}

bool VideoPipelineImpl::StartFlush() {
  CMALOG(kLogControl) << __FUNCTION__;
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    return false;
  }
  DCHECK_EQ(av_pipeline_impl_->GetState(), AvPipelineImpl::kPlaying);
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushing);
  return true;
}

void VideoPipelineImpl::Flush(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK_EQ(av_pipeline_impl_->GetState(), AvPipelineImpl::kFlushing);
  av_pipeline_impl_->Flush(
      base::Bind(&VideoPipelineImpl::OnFlushDone, weak_this_, status_cb));
}

void VideoPipelineImpl::BackendStopped() {
  CMALOG(kLogControl) << __FUNCTION__;
  av_pipeline_impl_->BackendStopped();
}

void VideoPipelineImpl::OnFlushDone(
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void VideoPipelineImpl::Stop() {
  CMALOG(kLogControl) << __FUNCTION__;
  av_pipeline_impl_->Stop();
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kStopped);
}

void VideoPipelineImpl::SetCdm(BrowserCdmCast* media_keys) {
  av_pipeline_impl_->SetCdm(media_keys);
}

void VideoPipelineImpl::OnBufferPushed(
    MediaPipelineBackend::BufferStatus status) {
  av_pipeline_impl_->OnBufferPushed(status);
}

void VideoPipelineImpl::OnEndOfStream() {
  if (!video_client_.av_pipeline_client.eos_cb.is_null())
    video_client_.av_pipeline_client.eos_cb.Run();
}

void VideoPipelineImpl::OnError() {
  if (!video_client_.av_pipeline_client.playback_error_cb.is_null()) {
    video_client_.av_pipeline_client.playback_error_cb.Run(
        ::media::PIPELINE_ERROR_COULD_NOT_RENDER);
  }
}

void VideoPipelineImpl::Initialize(
    const std::vector<::media::VideoDecoderConfig>& configs,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK_GT(configs.size(), 0u);
  for (const auto& config : configs) {
    CMALOG(kLogControl) << __FUNCTION__ << " "
                        << config.AsHumanReadableString();
  }

  if (frame_provider)
    SetCodedFrameProvider(frame_provider.Pass());

  if (configs.empty()) {
     status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
     return;
  }
  DCHECK(configs.size() <= 2);
  DCHECK(configs[0].IsValidConfig());
  VideoConfig video_config =
      DecoderConfigAdapter::ToCastVideoConfig(kPrimary, configs[0]);
  VideoConfig secondary_config;
  if (configs.size() == 2) {
    DCHECK(configs[1].IsValidConfig());
    secondary_config = DecoderConfigAdapter::ToCastVideoConfig(kSecondary,
                                                               configs[1]);
    video_config.additional_config = &secondary_config;
  }

  if (!video_decoder_->SetConfig(video_config)) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void VideoPipelineImpl::OnUpdateConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (video_config.IsValidConfig()) {
    CMALOG(kLogControl) << __FUNCTION__ << " id:" << id << " "
                        << video_config.AsHumanReadableString();

    bool success = video_decoder_->SetConfig(
        DecoderConfigAdapter::ToCastVideoConfig(id, video_config));
    if (!success &&
        !video_client_.av_pipeline_client.playback_error_cb.is_null()) {
      video_client_.av_pipeline_client.playback_error_cb.Run(
          ::media::PIPELINE_ERROR_DECODE);
    }
  }
}

void VideoPipelineImpl::OnNaturalSizeChanged(const Size& size) {
  if (av_pipeline_impl_->GetState() != AvPipelineImpl::kPlaying)
    return;

  if (!video_client_.natural_size_changed_cb.is_null()) {
    video_client_.natural_size_changed_cb.Run(
        gfx::Size(size.width, size.height));
  }
}

void VideoPipelineImpl::UpdateStatistics() {
  if (video_client_.av_pipeline_client.statistics_cb.is_null())
    return;

  MediaPipelineBackend::Decoder::Statistics device_stats;
  video_decoder_->GetStatistics(&device_stats);

  ::media::PipelineStatistics current_stats;
  current_stats.video_bytes_decoded = device_stats.decoded_bytes;
  current_stats.video_frames_decoded = device_stats.decoded_samples;
  current_stats.video_frames_dropped = device_stats.dropped_samples;

  ::media::PipelineStatistics delta_stats;
  delta_stats.video_bytes_decoded =
      current_stats.video_bytes_decoded - previous_stats_.video_bytes_decoded;
  delta_stats.video_frames_decoded =
      current_stats.video_frames_decoded - previous_stats_.video_frames_decoded;
  delta_stats.video_frames_dropped =
      current_stats.video_frames_dropped - previous_stats_.video_frames_dropped;

  previous_stats_ = current_stats;

  video_client_.av_pipeline_client.statistics_cb.Run(delta_stats);
}

}  // namespace media
}  // namespace chromecast
