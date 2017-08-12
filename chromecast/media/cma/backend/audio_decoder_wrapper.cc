// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"

#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

namespace chromecast {
namespace media {

AudioDecoderWrapper::AudioDecoderWrapper(
    MediaPipelineBackendManager* backend_manager,
    AudioDecoder* decoder,
    AudioContentType type)
    : backend_manager_(backend_manager),
      decoder_(decoder),
      content_type_(type),
      global_volume_multiplier_(1.0f),
      stream_volume_multiplier_(1.0f) {
  DCHECK(backend_manager_);
  DCHECK(decoder_);

  backend_manager_->AddAudioDecoder(this);
}

AudioDecoderWrapper::~AudioDecoderWrapper() {
  backend_manager_->RemoveAudioDecoder(this);
}

void AudioDecoderWrapper::SetGlobalVolumeMultiplier(float multiplier) {
  global_volume_multiplier_ = multiplier;
  decoder_->SetVolume(stream_volume_multiplier_ * global_volume_multiplier_);
}

void AudioDecoderWrapper::SetDelegate(Delegate* delegate) {
  decoder_->SetDelegate(delegate);
}

MediaPipelineBackend::BufferStatus AudioDecoderWrapper::PushBuffer(
    CastDecoderBuffer* buffer) {
  return decoder_->PushBuffer(buffer);
}

bool AudioDecoderWrapper::SetConfig(const AudioConfig& config) {
  return decoder_->SetConfig(config);
}

bool AudioDecoderWrapper::SetVolume(float multiplier) {
  stream_volume_multiplier_ = std::max(0.0f, std::min(multiplier, 1.0f));
  return decoder_->SetVolume(stream_volume_multiplier_ *
                             global_volume_multiplier_);
}

AudioDecoderWrapper::RenderingDelay AudioDecoderWrapper::GetRenderingDelay() {
  return decoder_->GetRenderingDelay();
}

void AudioDecoderWrapper::GetStatistics(Statistics* statistics) {
  decoder_->GetStatistics(statistics);
}

}  // namespace media
}  // namespace chromecast
