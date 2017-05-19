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

const int kUseDefaultFade = -1;
const int kMediaDuckFadeMs = 150;
const int kMediaUnduckFadeMs = 700;

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

float AudioSinkManager::VolumeInfo::GetEffectiveVolume() {
  return std::min(volume, limit);
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

  auto type = sink->content_type();
  if (sink->primary()) {
    sink->SetContentTypeVolume(volume_info_[type].GetEffectiveVolume(),
                               kUseDefaultFade);
  } else {
    sink->SetContentTypeVolume(volume_info_[type].volume, kUseDefaultFade);
  }
  sink->SetMuted(volume_info_[type].muted);

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

void AudioSinkManager::SetVolume(AudioContentType type, float level) {
  LOG(INFO) << __func__ << ": level=" << level
            << " type=" << GetAudioContentTypeName(type);

  base::AutoLock lock(lock_);

  volume_info_[type].volume = level;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  for (auto* sink : sinks_) {
    if (sink->content_type() != type) {
      continue;
    }
    if (sink->primary()) {
      sink->SetContentTypeVolume(effective_volume, kUseDefaultFade);
    } else {
      // Volume limits don't apply to effects streams.
      sink->SetContentTypeVolume(level, kUseDefaultFade);
    }
  }
}

void AudioSinkManager::SetMuted(AudioContentType type, bool muted) {
  base::AutoLock lock(lock_);

  LOG(INFO) << __func__ << ": muted=" << muted
            << " type=" << GetAudioContentTypeName(type);

  volume_info_[type].muted = muted;
  for (auto* sink : sinks_) {
    if (sink->content_type() == type) {
      sink->SetMuted(muted);
    }
  }
}

void AudioSinkManager::SetOutputLimit(AudioContentType type, float limit) {
  LOG(INFO) << __func__ << ": limit=" << limit
            << " type=" << GetAudioContentTypeName(type);

  base::AutoLock lock(lock_);

  volume_info_[type].limit = limit;
  float effective_volume = volume_info_[type].GetEffectiveVolume();
  int fade_ms = kUseDefaultFade;
  if (type == AudioContentType::kMedia) {
    if (limit >= 1.0f) {  // Unducking.
      fade_ms = kMediaUnduckFadeMs;
    } else {
      fade_ms = kMediaDuckFadeMs;
    }
  }
  for (auto* sink : sinks_) {
    // Volume limits don't apply to effects streams.
    if (sink->primary() && sink->content_type() == type) {
      sink->SetContentTypeVolume(effective_volume, fade_ms);
    }
  }
}

}  // namespace media
}  // namespace chromecast
