// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/media_pipeline_backend_alsa.h"

#include <limits>

#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/alsa/audio_decoder_alsa.h"
#include "chromecast/media/cma/backend/video_decoder_null.h"

namespace chromecast {
namespace media {

MediaPipelineBackendAlsa::MediaPipelineBackendAlsa(
    const MediaPipelineDeviceParams& params)
    : state_(kStateUninitialized), params_(params) {
}

MediaPipelineBackendAlsa::~MediaPipelineBackendAlsa() {
}

MediaPipelineBackendAlsa::AudioDecoder*
MediaPipelineBackendAlsa::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_.reset(new AudioDecoderAlsa(this));
  return audio_decoder_.get();
}

MediaPipelineBackendAlsa::VideoDecoder*
MediaPipelineBackendAlsa::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_.reset(new VideoDecoderNull());
  return video_decoder_.get();
}

bool MediaPipelineBackendAlsa::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    audio_decoder_->Initialize();
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendAlsa::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Start(start_pts))
    return false;
  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendAlsa::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused) << "Invalid state "
                                                            << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();

  state_ = kStateInitialized;
}

bool MediaPipelineBackendAlsa::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_ && !audio_decoder_->Pause())
    return false;
  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendAlsa::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendAlsa::SetPlaybackRate(float rate) {
  if (audio_decoder_) {
    return audio_decoder_->SetPlaybackRate(rate);
  }
  return true;
}

int64_t MediaPipelineBackendAlsa::GetCurrentPts() {
  if (audio_decoder_)
    return audio_decoder_->GetCurrentPts();
  return std::numeric_limits<int64_t>::min();
}

bool MediaPipelineBackendAlsa::Primary() const {
  return (params_.audio_type !=
          MediaPipelineDeviceParams::kAudioStreamSoundEffects);
}

std::string MediaPipelineBackendAlsa::DeviceId() const {
  return params_.device_id;
}

AudioContentType MediaPipelineBackendAlsa::ContentType() const {
  return params_.content_type;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
MediaPipelineBackendAlsa::GetTaskRunner() const {
  return static_cast<TaskRunnerImpl*>(params_.task_runner)->runner();
}

}  // namespace media
}  // namespace chromecast
