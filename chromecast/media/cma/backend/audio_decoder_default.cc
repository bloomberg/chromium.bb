// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_default.h"

#include "base/logging.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

AudioDecoderDefault::AudioDecoderDefault() : delegate_(nullptr) {}

AudioDecoderDefault::~AudioDecoderDefault() {}

void AudioDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus AudioDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);
  if (buffer->end_of_stream())
    delegate_->OnEndOfStream();
  return MediaPipelineBackend::kBufferSuccess;
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
