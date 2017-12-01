// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system.h"

#include "base/memory/ptr_util.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system_impl.h"

namespace media {

AudioSystem::~AudioSystem() = default;

// static
std::unique_ptr<AudioSystem> AudioSystem::CreateInstance() {
  DCHECK(AudioManager::Get()) << "AudioManager instance is not created";
  return std::make_unique<AudioSystemImpl>(AudioManager::Get());
}

}  // namespace media
