// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_default.h"

#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/media_sink_default.h"

namespace chromecast {
namespace media {

AudioDecoderDefault::AudioDecoderDefault() : delegate_(nullptr) {}

AudioDecoderDefault::~AudioDecoderDefault() {}

void AudioDecoderDefault::Start(base::TimeDelta start_pts) {
  DCHECK(!sink_);
  sink_ = base::MakeUnique<MediaSinkDefault>(delegate_, start_pts);
}

void AudioDecoderDefault::Stop() {
  DCHECK(sink_);
  sink_.reset();
}

void AudioDecoderDefault::SetPlaybackRate(float rate) {
  DCHECK(sink_);
  sink_->SetPlaybackRate(rate);
}

base::TimeDelta AudioDecoderDefault::GetCurrentPts() {
  DCHECK(sink_);
  return sink_->GetCurrentPts();
}

void AudioDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(!sink_);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus AudioDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(sink_);
  return sink_->PushBuffer(buffer);
}

void AudioDecoderDefault::GetStatistics(Statistics* statistics) {
}

bool AudioDecoderDefault::SetConfig(const AudioConfig& config) {
  return true;
}

bool AudioDecoderDefault::SetVolume(float multiplier) {
  return true;
}

AudioDecoderDefault::RenderingDelay AudioDecoderDefault::GetRenderingDelay() {
  return RenderingDelay();
}

}  // namespace media
}  // namespace chromecast
