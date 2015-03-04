// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"

#include "base/bind.h"
#include "chromecast/media/cma/backend/video_pipeline_device.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxVideoFrameSize = 1024 * 1024;
}

VideoPipelineImpl::VideoPipelineImpl(VideoPipelineDevice* video_device)
    : video_device_(video_device),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  av_pipeline_impl_ = new AvPipelineImpl(
      video_device_,
      base::Bind(&VideoPipelineImpl::OnUpdateConfig, base::Unretained(this)));
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
  CMALOG(kLogControl) << "VideoPipelineImpl::StartPlayingFrom t0="
                      << time.InMilliseconds();

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

void VideoPipelineImpl::Flush(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << "VideoPipelineImpl::Flush";
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  DCHECK_EQ(av_pipeline_impl_->GetState(), AvPipelineImpl::kPlaying);
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushing);
  av_pipeline_impl_->Flush(
      base::Bind(&VideoPipelineImpl::OnFlushDone, weak_this_, status_cb));
}

void VideoPipelineImpl::OnFlushDone(
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << "VideoPipelineImpl::OnFlushDone";
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void VideoPipelineImpl::Stop() {
  CMALOG(kLogControl) << "VideoPipelineImpl::Stop";
  av_pipeline_impl_->Stop();
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kStopped);
}

void VideoPipelineImpl::SetCdm(BrowserCdmCast* media_keys) {
  av_pipeline_impl_->SetCdm(media_keys);
}

void VideoPipelineImpl::SetClient(const VideoPipelineClient& client) {
  video_client_ = client;
  av_pipeline_impl_->SetClient(client.av_pipeline_client);
}

void VideoPipelineImpl::Initialize(
    const ::media::VideoDecoderConfig& video_config,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << "VideoPipelineImpl::Initialize "
                      << video_config.AsHumanReadableString();
  VideoPipelineDevice::VideoClient client;
  client.natural_size_changed_cb =
      base::Bind(&VideoPipelineImpl::OnNaturalSizeChanged, weak_this_);
  video_device_->SetVideoClient(client);
  if (frame_provider)
    SetCodedFrameProvider(frame_provider.Pass());

  if (!video_device_->SetConfig(video_config) ||
      !av_pipeline_impl_->Initialize()) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void VideoPipelineImpl::OnUpdateConfig(
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (video_config.IsValidConfig()) {
    CMALOG(kLogControl) << "VideoPipelineImpl::OnUpdateConfig "
                        << video_config.AsHumanReadableString();

    bool success = video_device_->SetConfig(video_config);
    if (!success &&
        !video_client_.av_pipeline_client.playback_error_cb.is_null()) {
      video_client_.av_pipeline_client.playback_error_cb.Run(
          ::media::PIPELINE_ERROR_DECODE);
    }
  }
}

void VideoPipelineImpl::OnNaturalSizeChanged(const gfx::Size& size) {
  if (av_pipeline_impl_->GetState() != AvPipelineImpl::kPlaying)
    return;

  if (!video_client_.natural_size_changed_cb.is_null())
    video_client_.natural_size_changed_cb.Run(size);
}

void VideoPipelineImpl::UpdateStatistics() {
  if (video_client_.av_pipeline_client.statistics_cb.is_null())
    return;

  MediaComponentDevice::Statistics device_stats;
  if (!video_device_->GetStatistics(&device_stats))
    return;

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
