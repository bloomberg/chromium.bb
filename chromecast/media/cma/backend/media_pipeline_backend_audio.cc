// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_audio.h"

#include <limits>

#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_null.h"

namespace chromecast {
namespace media {

MediaPipelineBackendAudio::MediaPipelineBackendAudio(
    const MediaPipelineDeviceParams& params)
    : state_(kStateUninitialized), params_(params) {}

MediaPipelineBackendAudio::~MediaPipelineBackendAudio() {}

MediaPipelineBackendAudio::AudioDecoder*
MediaPipelineBackendAudio::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_ = std::make_unique<AudioDecoderForMixer>(this);
  return audio_decoder_.get();
}

MediaPipelineBackendAudio::VideoDecoder*
MediaPipelineBackendAudio::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_ = std::make_unique<VideoDecoderNull>();
  return video_decoder_.get();
}

bool MediaPipelineBackendAudio::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    audio_decoder_->Initialize();
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendAudio::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Start(start_pts))
    return false;
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendAudio::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused)
      << "Invalid state " << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();

  state_ = kStateInitialized;
}

bool MediaPipelineBackendAudio::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_ && !audio_decoder_->Pause())
    return false;
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendAudio::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendAudio::SetPlaybackRate(float rate) {
  if (audio_decoder_) {
    return audio_decoder_->SetPlaybackRate(rate);
  }
  return true;
}

int64_t MediaPipelineBackendAudio::GetCurrentPts() {
  if (audio_decoder_)
    return audio_decoder_->GetCurrentPts();
  return std::numeric_limits<int64_t>::min();
}

bool MediaPipelineBackendAudio::Primary() const {
  return (params_.audio_type !=
          MediaPipelineDeviceParams::kAudioStreamSoundEffects);
}

std::string MediaPipelineBackendAudio::DeviceId() const {
  return params_.device_id;
}

AudioContentType MediaPipelineBackendAudio::ContentType() const {
  return params_.content_type;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
MediaPipelineBackendAudio::GetTaskRunner() const {
  return static_cast<TaskRunnerImpl*>(params_.task_runner)->runner();
}

}  // namespace media
}  // namespace chromecast
