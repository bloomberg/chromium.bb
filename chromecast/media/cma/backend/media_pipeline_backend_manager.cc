// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_features.h"
#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace {
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
constexpr int kAudioDecoderLimit = std::numeric_limits<int>::max();
#else
constexpr int kAudioDecoderLimit = 1;
#endif
}  // namespace

MediaPipelineBackendManager::MediaPipelineBackendManager(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)),
      playing_noneffects_audio_streams_count_(0),
      allow_volume_feedback_observers_(
          new base::ObserverListThreadSafe<AllowVolumeFeedbackObserver>()),
      global_volume_multipliers_({{AudioContentType::kMedia, 1.0f},
                                  {AudioContentType::kAlarm, 1.0f},
                                  {AudioContentType::kCommunication, 1.0f}},
                                 base::KEEP_FIRST_OF_DUPES),
      weak_factory_(this) {
  for (int i = 0; i < NUM_DECODER_TYPES; ++i) {
    decoder_count_[i] = 0;
  }
}

MediaPipelineBackendManager::~MediaPipelineBackendManager() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return base::MakeUnique<MediaPipelineBackendWrapper>(params, this);
}

bool MediaPipelineBackendManager::IncrementDecoderCount(DecoderType type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(type < NUM_DECODER_TYPES);
  const int limit = (type == AUDIO_DECODER) ? kAudioDecoderLimit : 1;
  if (decoder_count_[type] >= limit) {
    LOG(WARNING) << "Decoder limit reached for type " << type;
    return false;
  }

  ++decoder_count_[type];
  return true;
}

void MediaPipelineBackendManager::DecrementDecoderCount(DecoderType type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(type < NUM_DECODER_TYPES);
  DCHECK(decoder_count_[type] > 0);

  decoder_count_[type]--;
}

void MediaPipelineBackendManager::UpdatePlayingAudioCount(int change) {
  DCHECK(change == -1 || change == 1) << "bad count change: " << change;

  // Volume feedback sounds are only allowed when there are no non-effects
  // audio streams playing.
  bool prev_allow_feedback = (playing_noneffects_audio_streams_count_ == 0);
  playing_noneffects_audio_streams_count_ += change;
  DCHECK_GE(playing_noneffects_audio_streams_count_, 0);
  bool new_allow_feedback = (playing_noneffects_audio_streams_count_ == 0);

  if (new_allow_feedback != prev_allow_feedback) {
    allow_volume_feedback_observers_->Notify(
        FROM_HERE, &AllowVolumeFeedbackObserver::AllowVolumeFeedbackSounds,
        new_allow_feedback);
  }
}

void MediaPipelineBackendManager::AddAllowVolumeFeedbackObserver(
    AllowVolumeFeedbackObserver* observer) {
  allow_volume_feedback_observers_->AddObserver(observer);
}

void MediaPipelineBackendManager::RemoveAllowVolumeFeedbackObserver(
    AllowVolumeFeedbackObserver* observer) {
  allow_volume_feedback_observers_->RemoveObserver(observer);
}

void MediaPipelineBackendManager::LogicalPause(MediaPipelineBackend* backend) {
  MediaPipelineBackendWrapper* wrapper =
      static_cast<MediaPipelineBackendWrapper*>(backend);
  wrapper->LogicalPause();
}

void MediaPipelineBackendManager::LogicalResume(MediaPipelineBackend* backend) {
  MediaPipelineBackendWrapper* wrapper =
      static_cast<MediaPipelineBackendWrapper*>(backend);
  wrapper->LogicalResume();
}

void MediaPipelineBackendManager::SetGlobalVolumeMultiplier(
    AudioContentType type,
    float multiplier) {
  if (!media_task_runner_->BelongsToCurrentThread()) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPipelineBackendManager::SetGlobalVolumeMultiplier,
                   weak_factory_.GetWeakPtr(), type, multiplier));
    return;
  }

  DCHECK_GE(multiplier, 0.0f);
  global_volume_multipliers_[type] = multiplier;
  for (auto* a : audio_decoders_) {
    if (a->content_type() == type) {
      a->SetGlobalVolumeMultiplier(multiplier);
    }
  }
}

void MediaPipelineBackendManager::AddAudioDecoder(
    AudioDecoderWrapper* decoder) {
  DCHECK(decoder);
  audio_decoders_.insert(decoder);
  decoder->SetGlobalVolumeMultiplier(
      global_volume_multipliers_[decoder->content_type()]);
}

void MediaPipelineBackendManager::RemoveAudioDecoder(
    AudioDecoderWrapper* decoder) {
  audio_decoders_.erase(decoder);
}

}  // namespace media
}  // namespace chromecast
