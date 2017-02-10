// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system.h"

namespace media {

static AudioSystem* g_last_created = nullptr;

AudioSystem::~AudioSystem() {}

AudioSystem* AudioSystem::Get() {
  return g_last_created;
}

void AudioSystem::SetInstance(AudioSystem* audio_system) {
  DCHECK(audio_system);
  if (g_last_created && audio_system) {
    // We create multiple instances of AudioSystem only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioSystem detected";
  }
  g_last_created = audio_system;
}

void AudioSystem::ClearInstance(const AudioSystem* audio_system) {
  DCHECK(audio_system);
  if (g_last_created != audio_system) {
    // We create multiple instances of AudioSystem only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioSystem detected";
  } else {
    g_last_created = nullptr;
  }
}

}  // namespace media
