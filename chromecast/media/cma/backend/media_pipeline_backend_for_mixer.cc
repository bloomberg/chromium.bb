// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"

#include <time.h>
#include <limits>

#include "build/build_config.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

#if defined(OS_LINUX)
#include "chromecast/media/cma/backend/audio_buildflags.h"
#endif  // defined(OS_LINUX)

#if defined(OS_FUCHSIA)
#include <zircon/syscalls.h>
#endif  // defined(OS_FUCHSIA)

namespace chromecast {
namespace media {

MediaPipelineBackendForMixer::MediaPipelineBackendForMixer(
    const MediaPipelineDeviceParams& params)
    : state_(kStateUninitialized), params_(params) {}

MediaPipelineBackendForMixer::~MediaPipelineBackendForMixer() {}

MediaPipelineBackendForMixer::AudioDecoder*
MediaPipelineBackendForMixer::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_ = std::make_unique<AudioDecoderForMixer>(this);
  return audio_decoder_.get();
}

MediaPipelineBackendForMixer::VideoDecoder*
MediaPipelineBackendForMixer::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_ = VideoDecoderForMixer::Create(params_);
  DCHECK(video_decoder_.get());
  return video_decoder_.get();
}

bool MediaPipelineBackendForMixer::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    audio_decoder_->Initialize();
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendForMixer::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);
  if (audio_decoder_ && !audio_decoder_->Start(start_pts))
    return false;
  if (video_decoder_ && !video_decoder_->Start(start_pts, true))
    return false;

  state_ = kStatePlaying;
  return true;
}

void MediaPipelineBackendForMixer::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused)
      << "Invalid state " << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();
  if (video_decoder_)
    video_decoder_->Stop();

  state_ = kStateInitialized;
}

bool MediaPipelineBackendForMixer::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (audio_decoder_ && !audio_decoder_->Pause())
    return false;
  if (video_decoder_ && !video_decoder_->Pause())
    return false;

  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendForMixer::Resume() {
  DCHECK_EQ(kStatePaused, state_);
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  if (video_decoder_ && !video_decoder_->Resume())
    return false;

  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendForMixer::SetPlaybackRate(float rate) {
  if (audio_decoder_) {
    return audio_decoder_->SetPlaybackRate(rate);
  }
  return true;
}

int64_t MediaPipelineBackendForMixer::GetCurrentPts() {
  if (audio_decoder_)
    return audio_decoder_->GetCurrentPts();
  return std::numeric_limits<int64_t>::min();
}

bool MediaPipelineBackendForMixer::Primary() const {
  return (params_.audio_type !=
          MediaPipelineDeviceParams::kAudioStreamSoundEffects);
}

std::string MediaPipelineBackendForMixer::DeviceId() const {
  return params_.device_id;
}

AudioContentType MediaPipelineBackendForMixer::ContentType() const {
  return params_.content_type;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
MediaPipelineBackendForMixer::GetTaskRunner() const {
  return static_cast<TaskRunnerImpl*>(params_.task_runner)->runner();
}

#if defined(OS_LINUX)
int64_t MediaPipelineBackendForMixer::MonotonicClockNow() const {
  timespec now = {0, 0};
#if BUILDFLAG(ALSA_MONOTONIC_RAW_TSTAMPS)
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#else
  clock_gettime(CLOCK_MONOTONIC, &now);
#endif
  return static_cast<int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}
#elif defined(OS_FUCHSIA)
int64_t MediaPipelineBackendForMixer::MonotonicClockNow() const {
  return zx_clock_get(ZX_CLOCK_MONOTONIC) / 1000;
}
#endif

}  // namespace media
}  // namespace chromecast
