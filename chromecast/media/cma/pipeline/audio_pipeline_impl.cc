// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"

#include "base/bind.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "chromecast/public/media/decoder_config.h"
#include "media/base/audio_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxAudioFrameSize = 32 * 1024;
}

AudioPipelineImpl::AudioPipelineImpl(
    MediaPipelineBackend::AudioDecoder* decoder,
    const AvPipelineClient& client)
    : decoder_(decoder), audio_client_(client), weak_factory_(this) {
  av_pipeline_impl_.reset(new AvPipelineImpl(
      decoder_,
      base::Bind(&AudioPipelineImpl::OnUpdateConfig, base::Unretained(this))));
  weak_this_ = weak_factory_.GetWeakPtr();
}

AudioPipelineImpl::~AudioPipelineImpl() {
}

void AudioPipelineImpl::SetCodedFrameProvider(
    scoped_ptr<CodedFrameProvider> frame_provider) {
  av_pipeline_impl_->SetCodedFrameProvider(
      frame_provider.Pass(), kAppAudioBufferSize, kMaxAudioFrameSize);
}

bool AudioPipelineImpl::StartPlayingFrom(
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

void AudioPipelineImpl::Flush(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  DCHECK_EQ(av_pipeline_impl_->GetState(), AvPipelineImpl::kPlaying);
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushing);
  av_pipeline_impl_->Flush(
      base::Bind(&AudioPipelineImpl::OnFlushDone, weak_this_, status_cb));
}

void AudioPipelineImpl::OnFlushDone(
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  if (av_pipeline_impl_->GetState() == AvPipelineImpl::kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void AudioPipelineImpl::Stop() {
  CMALOG(kLogControl) << __FUNCTION__;
  av_pipeline_impl_->Stop();
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kStopped);
}

void AudioPipelineImpl::SetCdm(BrowserCdmCast* media_keys) {
  av_pipeline_impl_->SetCdm(media_keys);
}

void AudioPipelineImpl::Initialize(
    const ::media::AudioDecoderConfig& audio_config,
    scoped_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__ << " "
                      << audio_config.AsHumanReadableString();
  if (frame_provider)
    SetCodedFrameProvider(frame_provider.Pass());

  DCHECK(audio_config.IsValidConfig());
  if (!decoder_->SetConfig(
          DecoderConfigAdapter::ToCastAudioConfig(kPrimary, audio_config))) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  av_pipeline_impl_->TransitionToState(AvPipelineImpl::kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void AudioPipelineImpl::SetVolume(float volume) {
  decoder_->SetVolume(volume);
}

void AudioPipelineImpl::OnBufferPushed(
    MediaPipelineBackend::BufferStatus status) {
  av_pipeline_impl_->OnBufferPushed(status);
}

void AudioPipelineImpl::OnEndOfStream() {
  if (!audio_client_.eos_cb.is_null())
    audio_client_.eos_cb.Run();
}

void AudioPipelineImpl::OnError() {
  if (!audio_client_.playback_error_cb.is_null()) {
    audio_client_.playback_error_cb.Run(
        ::media::PIPELINE_ERROR_COULD_NOT_RENDER);
  }
}

void AudioPipelineImpl::OnUpdateConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (audio_config.IsValidConfig()) {
    CMALOG(kLogControl) << __FUNCTION__ << " id:" << id << " "
                        << audio_config.AsHumanReadableString();

    bool success = decoder_->SetConfig(
        DecoderConfigAdapter::ToCastAudioConfig(id, audio_config));
    if (!success && !audio_client_.playback_error_cb.is_null())
      audio_client_.playback_error_cb.Run(::media::PIPELINE_ERROR_DECODE);
  }
}

void AudioPipelineImpl::UpdateStatistics() {
  if (audio_client_.statistics_cb.is_null())
    return;

  MediaPipelineBackend::Decoder::Statistics device_stats;
  decoder_->GetStatistics(&device_stats);

  ::media::PipelineStatistics current_stats;
  current_stats.audio_bytes_decoded = device_stats.decoded_bytes;

  ::media::PipelineStatistics delta_stats;
  delta_stats.audio_bytes_decoded =
      current_stats.audio_bytes_decoded - previous_stats_.audio_bytes_decoded;

  previous_stats_ = current_stats;

  audio_client_.statistics_cb.Run(delta_stats);
}

}  // namespace media
}  // namespace chromecast
