// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processors/governor.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

chromecast::media::AudioPostProcessor* AudioPostProcessorShlib_Create(
    const std::string& config,
    int channels) {
  return static_cast<chromecast::media::AudioPostProcessor*>(
      new chromecast::media::Governor(config, channels));
}
