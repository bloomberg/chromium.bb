// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_factory.h"

#include "chromecast/media/audio/cast_audio_manager.h"

namespace chromecast {
namespace media {

CastAudioManagerFactory::CastAudioManagerFactory() {}

CastAudioManagerFactory::~CastAudioManagerFactory() {}

::media::AudioManager* CastAudioManagerFactory::CreateInstance(
    ::media::AudioLogFactory* audio_log_factory) {
  return new CastAudioManager(audio_log_factory);
}

}  // namespace media
}  // namespace chromecast
