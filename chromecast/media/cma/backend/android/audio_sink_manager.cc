// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

#include <algorithm>

#include "base/lazy_instance.h"
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

base::LazyInstance<AudioSinkManagerInstance>::DestructorAtExit
    g_sink_manager_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

float AudioSinkManager::VolumeInfo::GetLimiterMultiplier() {
  // Goal: multiplier * volume = min(volume, limit).
  if (volume == 0)
    return 0.0f;
  if (volume < limit)
    return 1.0f;
  return limit / volume;
}

// static
AudioSinkManager* AudioSinkManager::Get() {
  return g_sink_manager_instance.Pointer();
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

void AudioSinkManager::SetTypeVolume(AudioContentType type, float level) {
  LOG(INFO) << __func__ << ": Set volume for " << GetAudioContentTypeName(type)
            << " to level=" << level;
  base::AutoLock lock(lock_);
  volume_info_[type].volume = level;
  // Since the type volume changed we need to reflect that in the limiter
  // multipliers.
  UpdateAllLimiterMultipliers(type);
}

void AudioSinkManager::SetOutputLimit(AudioContentType type, float limit) {
  LOG(INFO) << __func__ << ": limit=" << limit
            << " type=" << GetAudioContentTypeName(type);
  base::AutoLock lock(lock_);
  volume_info_[type].limit = limit;
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
    sink->SetLimiterVolumeMultiplier(volume_info_[type].GetLimiterMultiplier());
  } else {
    // Volume limits don't apply to effects streams.
    sink->SetLimiterVolumeMultiplier(1.0f);
  }
}

}  // namespace media
}  // namespace chromecast
