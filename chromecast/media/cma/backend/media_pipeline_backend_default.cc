// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/audio_decoder_default.h"
#include "chromecast/media/cma/backend/video_decoder_default.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

MediaPipelineBackendDefault::MediaPipelineBackendDefault()
    : state_(kStateUninitialized), rate_(1.0f) {}

MediaPipelineBackendDefault::~MediaPipelineBackendDefault() {
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendDefault::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  DCHECK(!audio_decoder_);
  audio_decoder_ = base::MakeUnique<AudioDecoderDefault>();
  return audio_decoder_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendDefault::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  DCHECK(!video_decoder_);
  video_decoder_ = base::MakeUnique<VideoDecoderDefault>();
  return video_decoder_.get();
}

bool MediaPipelineBackendDefault::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendDefault::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (!audio_decoder_ && !video_decoder_)
    return false;

  if (audio_decoder_) {
    audio_decoder_->Start(base::TimeDelta::FromMicroseconds(start_pts));
    audio_decoder_->SetPlaybackRate(rate_);
  }
  if (video_decoder_) {
    video_decoder_->Start(base::TimeDelta::FromMicroseconds(start_pts));
    video_decoder_->SetPlaybackRate(rate_);
  }
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendDefault::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused);
  if (audio_decoder_)
    audio_decoder_->Stop();
  if (video_decoder_)
    video_decoder_->Stop();
  state_ = kStateInitialized;
}

bool MediaPipelineBackendDefault::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_)
    audio_decoder_->SetPlaybackRate(0.0f);
  if (video_decoder_)
    video_decoder_->SetPlaybackRate(0.0f);
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendDefault::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_)
    audio_decoder_->SetPlaybackRate(rate_);
  if (video_decoder_)
    video_decoder_->SetPlaybackRate(rate_);
  state_ = kStatePlaying;
  return true;
}

int64_t MediaPipelineBackendDefault::GetCurrentPts() {
  base::TimeDelta current_pts = ::media::kNoTimestamp;

  if (audio_decoder_ && video_decoder_) {
    current_pts = std::min(audio_decoder_->GetCurrentPts(),
                           video_decoder_->GetCurrentPts());
  } else if (audio_decoder_) {
    current_pts = audio_decoder_->GetCurrentPts();
  } else if (video_decoder_) {
    current_pts = video_decoder_->GetCurrentPts();
  }

  return current_pts.InMicroseconds();
}

bool MediaPipelineBackendDefault::SetPlaybackRate(float rate) {
  DCHECK_GT(rate, 0.0f);
  rate_ = rate;

  if (state_ == kStatePlaying) {
    if (audio_decoder_)
      audio_decoder_->SetPlaybackRate(rate_);
    if (video_decoder_)
      video_decoder_->SetPlaybackRate(rate_);
  }

  return true;
}

}  // namespace media
}  // namespace chromecast
