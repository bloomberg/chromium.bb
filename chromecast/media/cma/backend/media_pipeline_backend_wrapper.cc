// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"

#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

namespace chromecast {
namespace media {

using DecoderType = MediaPipelineBackendManager::DecoderType;

MediaPipelineBackendWrapper::MediaPipelineBackendWrapper(
    std::unique_ptr<MediaPipelineBackend> backend,
    int stream_type,
    float stream_type_volume,
    MediaPipelineBackendManager* backend_manager)
    : backend_(std::move(backend)),
      stream_type_(stream_type),
      audio_decoder_wrapper_(nullptr),
      stream_type_volume_(stream_type_volume),
      is_initialized_(false),
      have_video_decoder_(false),
      backend_manager_(backend_manager) {
  DCHECK(backend_);
}

MediaPipelineBackendWrapper::~MediaPipelineBackendWrapper() {
  backend_manager_->OnMediaPipelineBackendDestroyed(this);

  if (audio_decoder_wrapper_)
    backend_manager_->DecrementDecoderCount(DecoderType::AUDIO_DECODER);
  if (have_video_decoder_)
    backend_manager_->DecrementDecoderCount(DecoderType::VIDEO_DECODER);
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendWrapper::CreateAudioDecoder() {
  DCHECK(!is_initialized_);
  if (audio_decoder_wrapper_)
    return nullptr;

  if (!backend_manager_->IncrementDecoderCount(DecoderType::AUDIO_DECODER))
    return nullptr;

  audio_decoder_wrapper_.reset(
      new AudioDecoderWrapper(backend_->CreateAudioDecoder()));
  return audio_decoder_wrapper_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendWrapper::CreateVideoDecoder() {
  DCHECK(!is_initialized_);
  DCHECK(!have_video_decoder_);

  if (!backend_manager_->IncrementDecoderCount(DecoderType::VIDEO_DECODER))
    return nullptr;
  have_video_decoder_ = true;

  return backend_->CreateVideoDecoder();
}

bool MediaPipelineBackendWrapper::Initialize() {
  DCHECK(!is_initialized_);
  is_initialized_ = backend_->Initialize();
  if (is_initialized_ && audio_decoder_wrapper_)
    audio_decoder_wrapper_->SetStreamTypeVolume(stream_type_volume_);

  return is_initialized_;
}

bool MediaPipelineBackendWrapper::Start(int64_t start_pts) {
  return backend_->Start(start_pts);
}

void MediaPipelineBackendWrapper::Stop() {
  backend_->Stop();
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
