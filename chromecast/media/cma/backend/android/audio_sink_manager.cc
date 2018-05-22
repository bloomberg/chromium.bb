// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"

namespace chromecast {
namespace media {

namespace {

class AudioSinkManagerInstance : public AudioSinkManager {
 public:
  AudioSinkManagerInstance() {}
  ~AudioSinkManagerInstance() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioSinkManagerInstance);
};

}  // namespace

// static
AudioSinkManager* AudioSinkManager::Get() {
  static base::NoDestructor<AudioSinkManagerInstance> sink_manager_instance;
  return sink_manager_instance.get();
}

// static
AudioSinkAndroid::SinkType AudioSinkManager::GetDefaultSinkType() {
  return AudioSinkAndroid::kSinkTypeJavaBased;
}

// static
bool AudioSinkManager::GetSessionIds(int* media_id, int* communication_id) {
  return AudioSinkAndroid::GetSessionIds(GetDefaultSinkType(), media_id,
                                         communication_id);
}

AudioSinkManager::AudioSinkManager() {}
AudioSinkManager::~AudioSinkManager() {}

void AudioSinkManager::Add(AudioSinkAndroid* sink) {
  DCHECK(sink);

  LOG(INFO) << __func__ << " sink(" << sink << "): id=" << sink->device_id()
            << " primary=" << sink->primary()
            << " type=" << sink->GetContentTypeName();

  base::AutoLock lock(lock_);
  UpdateLimiterMultiplier(sink);
  sinks_.push_back(sink);
}

void AudioSinkManager::Remove(AudioSinkAndroid* sink) {
  DCHECK(sink);

  LOG(INFO) << __func__ << " sink(" << sink << "): id=" << sink->device_id()
            << " type=" << sink->GetContentTypeName();

  base::AutoLock lock(lock_);

  auto it = std::find(sinks_.begin(), sinks_.end(), sink);
  if (it == sinks_.end()) {
    LOG(WARNING) << __func__ << ": Cannot find sink";
    return;
  }
  sinks_.erase(it);
}

void AudioSinkManager::SetTypeVolumeDb(AudioContentType type, float level_db) {
  LOG(INFO) << __func__ << ": level_db=" << level_db
            << " type=" << GetAudioContentTypeName(type);
  base::AutoLock lock(lock_);
  volume_info_[type].volume_db = level_db;
  // Since the type volume changed we need to reflect that in the limiter
  // multipliers.
  UpdateAllLimiterMultipliers(type);
}

void AudioSinkManager::SetOutputLimitDb(AudioContentType type, float limit_db) {
  LOG(INFO) << __func__ << ": limit_db=" << limit_db
            << " type=" << GetAudioContentTypeName(type);
  base::AutoLock lock(lock_);
  volume_info_[type].limit_db = limit_db;
  UpdateAllLimiterMultipliers(type);
}

void AudioSinkManager::UpdateAllLimiterMultipliers(AudioContentType type) {
  for (auto* sink : sinks_) {
    if (sink->content_type() == type)
      UpdateLimiterMultiplier(sink);
  }
}

void AudioSinkManager::UpdateLimiterMultiplier(AudioSinkAndroid* sink) {
  AudioContentType type = sink->content_type();
  if (sink->primary()) {
    sink->SetLimiterVolumeMultiplier(GetLimiterMultiplier(type));
  } else {
    // Volume limits don't apply to effects streams.
    sink->SetLimiterVolumeMultiplier(1.0f);
  }
}

float AudioSinkManager::GetLimiterMultiplier(AudioContentType type) {
  // Set multiplier so the effective volume is min(level_db, limit_db).
  VolumeInfo v = volume_info_[type];
  if (v.volume_db <= v.limit_db)
    return 1.0f;
  return std::pow(10, (v.limit_db - v.volume_db) / 20);
}

}  // namespace media
}  // namespace chromecast
