// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processors/saturated_gain.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

chromecast::media::AudioPostProcessor* AudioPostProcessorShlib_Create(
    const std::string& config,
    int channels) {
  return new chromecast::media::SaturatedGain(config, channels);
}
