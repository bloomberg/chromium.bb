// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace media {
namespace {
AudioManager* g_last_created = NULL;
}

// Forward declaration of the platform specific AudioManager factory function.
AudioManager* CreateAudioManager();

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
  CHECK(g_last_created == NULL || g_last_created == this);
  g_last_created = NULL;
}

// static
AudioManager* AudioManager::Create() {
  CHECK(g_last_created == NULL);
  g_last_created = CreateAudioManager();
  return g_last_created;
}

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

}  // namespace media
