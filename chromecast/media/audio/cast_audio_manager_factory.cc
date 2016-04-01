// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_factory.h"

#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

namespace chromecast {
namespace media {

CastAudioManagerFactory::CastAudioManagerFactory(
    MediaPipelineBackendManager* backend_manager)
    : backend_manager_(backend_manager) {
}

CastAudioManagerFactory::~CastAudioManagerFactory() {}

::media::AudioManager* CastAudioManagerFactory::CreateInstance(
    ::media::AudioLogFactory* audio_log_factory) {
  return new CastAudioManager(audio_log_factory, backend_manager_);
}

}  // namespace media
}  // namespace chromecast
