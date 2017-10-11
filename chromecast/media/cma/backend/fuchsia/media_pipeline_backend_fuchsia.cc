// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/media_pipeline_backend_fuchsia.h"

#include "chromecast/media/cma/backend/fuchsia/audio_decoder_fuchsia.h"
#include "chromecast/media/cma/backend/video_decoder_null.h"

namespace chromecast {
namespace media {

MediaPipelineBackendFuchsia::MediaPipelineBackendFuchsia(
    const MediaPipelineDeviceParams& params)
    : params_(params) {}

MediaPipelineBackendFuchsia::~MediaPipelineBackendFuchsia() {}

MediaPipelineBackendFuchsia::AudioDecoder*
MediaPipelineBackendFuchsia::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_ = std::make_unique<AudioDecoderFuchsia>();
  return audio_decoder_.get();
}

MediaPipelineBackendFuchsia::VideoDecoder*
MediaPipelineBackendFuchsia::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_ = std::make_unique<VideoDecoderNull>();
  return video_decoder_.get();
}

bool MediaPipelineBackendFuchsia::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Initialize())
    return false;
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendFuchsia::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Start(start_pts))
    return false;
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendFuchsia::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused)
      << "Invalid state: " << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();

  state_ = kStateInitialized;
}

bool MediaPipelineBackendFuchsia::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_ && !audio_decoder_->Pause())
    return false;
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendFuchsia::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendFuchsia::SetPlaybackRate(float rate) {
  if (audio_decoder_)
    return audio_decoder_->SetPlaybackRate(rate);
  return true;
}

int64_t MediaPipelineBackendFuchsia::GetCurrentPts() {
  if (audio_decoder_)
    return audio_decoder_->GetCurrentPts();
  return std::numeric_limits<int64_t>::min();
}

}  // namespace media
}  // namespace chromecast
