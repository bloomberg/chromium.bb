// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

using DecoderType = MediaPipelineBackendManager::DecoderType;

MediaPipelineBackendWrapper::MediaPipelineBackendWrapper(
    const media::MediaPipelineDeviceParams& params,
    MediaPipelineBackendManager* backend_manager)
    : backend_(base::WrapUnique(
          media::CastMediaShlib::CreateMediaPipelineBackend(params))),
      backend_manager_(backend_manager),
      content_type_(params.content_type),
      sfx_backend_(params.audio_type ==
                   media::MediaPipelineDeviceParams::kAudioStreamSoundEffects),
      have_video_decoder_(false),
      playing_(false) {
  DCHECK(backend_);
  DCHECK(backend_manager_);
}

MediaPipelineBackendWrapper::~MediaPipelineBackendWrapper() {
  if (audio_decoder_)
    backend_manager_->DecrementDecoderCount(
        sfx_backend_ ? DecoderType::SFX_DECODER : DecoderType::AUDIO_DECODER);
  if (have_video_decoder_)
    backend_manager_->DecrementDecoderCount(DecoderType::VIDEO_DECODER);

  if (playing_) {
    LOG(WARNING) << "Destroying media backend while still in 'playing' state";
    if (audio_decoder_ && !sfx_backend_) {
      backend_manager_->UpdatePlayingAudioCount(-1);
    }
  }
}

void MediaPipelineBackendWrapper::LogicalPause() {
  SetPlaying(false);
}

void MediaPipelineBackendWrapper::LogicalResume() {
  SetPlaying(true);
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendWrapper::CreateAudioDecoder() {
  DCHECK(!audio_decoder_);

  if (!backend_manager_->IncrementDecoderCount(
          sfx_backend_ ? DecoderType::SFX_DECODER : DecoderType::AUDIO_DECODER))
    return nullptr;
  MediaPipelineBackend::AudioDecoder* real_decoder =
      backend_->CreateAudioDecoder();
  if (!real_decoder) {
    return nullptr;
  }
  audio_decoder_ = base::MakeUnique<AudioDecoderWrapper>(
      backend_manager_, real_decoder, content_type_);
  return audio_decoder_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendWrapper::CreateVideoDecoder() {
  DCHECK(!have_video_decoder_);

  if (!backend_manager_->IncrementDecoderCount(DecoderType::VIDEO_DECODER))
    return nullptr;
  have_video_decoder_ = true;

  return backend_->CreateVideoDecoder();
}

bool MediaPipelineBackendWrapper::Initialize() {
  return backend_->Initialize();
}

bool MediaPipelineBackendWrapper::Start(int64_t start_pts) {
  if (!backend_->Start(start_pts)) {
    return false;
  }
  SetPlaying(true);
  return true;
}

void MediaPipelineBackendWrapper::Stop() {
  backend_->Stop();
  SetPlaying(false);
}

bool MediaPipelineBackendWrapper::Pause() {
  if (!backend_->Pause()) {
    return false;
  }
  SetPlaying(false);
  return true;
}

bool MediaPipelineBackendWrapper::Resume() {
  if (!backend_->Resume()) {
    return false;
  }
  SetPlaying(true);
  return true;
}

int64_t MediaPipelineBackendWrapper::GetCurrentPts() {
  return backend_->GetCurrentPts();
}

bool MediaPipelineBackendWrapper::SetPlaybackRate(float rate) {
  return backend_->SetPlaybackRate(rate);
}

void MediaPipelineBackendWrapper::SetPlaying(bool playing) {
  if (playing == playing_) {
    return;
  }
  playing_ = playing;
  if (audio_decoder_ && !sfx_backend_) {
    backend_manager_->UpdatePlayingAudioCount(playing_ ? 1 : -1);
  }
}

}  // namespace media
}  // namespace chromecast
