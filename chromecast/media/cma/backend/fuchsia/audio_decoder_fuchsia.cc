// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/audio_decoder_fuchsia.h"

#include "base/memory/ref_counted.h"

#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace chromecast {
namespace media {

AudioDecoderFuchsia::AudioDecoderFuchsia() {}
AudioDecoderFuchsia::~AudioDecoderFuchsia() {}

// static
int64_t MediaPipelineBackend::AudioDecoder::GetMinimumBufferedTime(
    const AudioConfig& config) {
  NOTIMPLEMENTED();
  return 2000000;  // 200ms
}

bool AudioDecoderFuchsia::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return true;
}

bool AudioDecoderFuchsia::Start(int64_t start_pts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return false;
}

void AudioDecoderFuchsia::Stop() {
  NOTIMPLEMENTED();
}

bool AudioDecoderFuchsia::Pause() {
  NOTIMPLEMENTED();
  return false;
}

bool AudioDecoderFuchsia::Resume() {
  NOTIMPLEMENTED();
  return false;
}

bool AudioDecoderFuchsia::SetPlaybackRate(float rate) {
  NOTIMPLEMENTED();
  return false;
}

int64_t AudioDecoderFuchsia::GetCurrentPts() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

bool AudioDecoderFuchsia::SetConfig(const AudioConfig& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return IsValidConfig(config);
}

bool AudioDecoderFuchsia::SetVolume(float multiplier) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return true;
}

AudioDecoderFuchsia::RenderingDelay AudioDecoderFuchsia::GetRenderingDelay() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return RenderingDelay();
}

void AudioDecoderFuchsia::GetStatistics(Statistics* statistics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  *statistics = stats_;
}

void AudioDecoderFuchsia::SetDelegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

AudioDecoderFuchsia::BufferStatus AudioDecoderFuchsia::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<DecoderBufferBase> buffer_base(
      static_cast<DecoderBufferBase*>(buffer));
  NOTIMPLEMENTED();
  return MediaPipelineBackend::kBufferSuccess;
}

}  // namespace media
}  // namespace chromecast
