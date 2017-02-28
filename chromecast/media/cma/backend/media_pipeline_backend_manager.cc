// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <algorithm>
#include <limits>

#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_features.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {
namespace {
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
constexpr int kAudioDecoderLimit = std::numeric_limits<int>::max();
#else
constexpr int kAudioDecoderLimit = 2;
#endif
}  // namespace

MediaPipelineBackendManager::MediaPipelineBackendManager(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)) {
  DCHECK_EQ(2, NUM_DECODER_TYPES);
  decoder_count_[AUDIO_DECODER] = 0;
  decoder_count_[VIDEO_DECODER] = 0;
}

MediaPipelineBackendManager::~MediaPipelineBackendManager() {
}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  return CreateMediaPipelineBackend(params, 0);
}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params,
    int stream_type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "Creating a " << params.device_id << " stream.";
  std::unique_ptr<MediaPipelineBackend> backend_ptr(
      new MediaPipelineBackendWrapper(
          base::WrapUnique(
              media::CastMediaShlib::CreateMediaPipelineBackend(params)),
          stream_type, GetVolumeMultiplier(stream_type), this));
  media_pipeline_backends_.push_back(backend_ptr.get());
  return backend_ptr;
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

void MediaPipelineBackendManager::OnMediaPipelineBackendDestroyed(
    const MediaPipelineBackend* backend) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_pipeline_backends_.erase(
      std::remove(media_pipeline_backends_.begin(),
                  media_pipeline_backends_.end(), backend),
      media_pipeline_backends_.end());
}

void MediaPipelineBackendManager::SetVolumeMultiplier(int stream_type,
                                                      float volume) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  volume = std::max(0.0f, std::min(volume, 1.0f));
  volume_by_stream_type_[stream_type] = volume;

  // Set volume for each open media pipeline backends.
  for (auto it = media_pipeline_backends_.begin();
       it != media_pipeline_backends_.end(); it++) {
    MediaPipelineBackendWrapper* wrapper =
        static_cast<MediaPipelineBackendWrapper*>(*it);
    if (wrapper->GetStreamType() == stream_type)
      wrapper->SetStreamTypeVolume(volume);
  }
}

float MediaPipelineBackendManager::GetVolumeMultiplier(int stream_type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  auto it = volume_by_stream_type_.find(stream_type);
  if (it == volume_by_stream_type_.end()) {
    return 1.0;
  } else {
    return it->second;
  }
}

}  // namespace media
}  // namespace chromecast
