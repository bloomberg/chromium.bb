// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"

#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

namespace chromecast {
namespace media {

MediaPipelineBackendWrapper::MediaPipelineBackendWrapper(
    MediaPipelineBackend* backend,
    int stream_type,
    float stream_type_volume)
    : backend_(backend),
      stream_type_(stream_type),
      audio_decoder_wrapper_(nullptr),
      stream_type_volume_(stream_type_volume),
      is_initialized_(false) {
  DCHECK(backend_);
}

MediaPipelineBackendWrapper::~MediaPipelineBackendWrapper() {
  MediaPipelineBackendManager::OnMediaPipelineBackendDestroyed(this);
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendWrapper::CreateAudioDecoder() {
  if (audio_decoder_wrapper_)
    return nullptr;

  audio_decoder_wrapper_.reset(
      new AudioDecoderWrapper(backend_->CreateAudioDecoder()));
  return audio_decoder_wrapper_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendWrapper::CreateVideoDecoder() {
  return backend_->CreateVideoDecoder();
}

bool MediaPipelineBackendWrapper::Initialize() {
  is_initialized_ = backend_->Initialize();
  if (is_initialized_)
    audio_decoder_wrapper_->SetStreamTypeVolume(stream_type_volume_);

  return is_initialized_;
}

bool MediaPipelineBackendWrapper::Start(int64_t start_pts) {
  return backend_->Start(start_pts);
}

bool MediaPipelineBackendWrapper::Stop() {
  return backend_->Stop();
}

bool MediaPipelineBackendWrapper::Pause() {
  return backend_->Pause();
}

bool MediaPipelineBackendWrapper::Resume() {
  return backend_->Resume();
}

int64_t MediaPipelineBackendWrapper::GetCurrentPts() {
  return backend_->GetCurrentPts();
}

bool MediaPipelineBackendWrapper::SetPlaybackRate(float rate) {
  return backend_->SetPlaybackRate(rate);
}

int MediaPipelineBackendWrapper::GetStreamType() const {
  return stream_type_;
}

void MediaPipelineBackendWrapper::SetStreamTypeVolume(
    float stream_type_volume) {
  stream_type_volume_ = stream_type_volume;
  if (is_initialized_ && audio_decoder_wrapper_)
    audio_decoder_wrapper_->SetStreamTypeVolume(stream_type_volume_);
}

}  // namespace media
}  // namespace chromecast
