// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processors/governor.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/base/slew_volume.h"

namespace chromecast {
namespace media {

namespace {
const int kNoSampleRate = -1;
const int kNoVolume = -1;

// Configuration strings:
const char kOnsetVolumeKey[] = "onset_volume";
const char kClampMultiplierKey[] = "clamp_multiplier";

}  // namespace

Governor::Governor(const std::string& config, int channels)
    : channels_(channels),
      sample_rate_(kNoSampleRate),
      volume_(kNoVolume),
      slew_volume_() {
  auto config_dict = base::DictionaryValue::From(DeserializeFromJson(config));
  CHECK(config_dict) << "Governor config is not valid json: " << config;
  CHECK(config_dict->GetDouble(kOnsetVolumeKey, &onset_volume_));
  CHECK(config_dict->GetDouble(kClampMultiplierKey, &clamp_multiplier_));
  slew_volume_.SetVolume(1.0);
  LOG(INFO) << "Created a governor: onset_volume = " << onset_volume_
            << ", clamp_multiplier = " << clamp_multiplier_;
}

Governor::~Governor() = default;

bool Governor::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
  slew_volume_.SetSampleRate(sample_rate);
  return true;
}

int Governor::ProcessFrames(float* data,
                            int frames,
                            float volume,
                            float volume_dbfs) {
  if (volume != volume_) {
    volume_ = volume;
    slew_volume_.SetVolume(GetGovernorMultiplier());
  }

  slew_volume_.ProcessFMUL(false, data, frames, channels_, data);

  return 0;  // No delay in this pipeline.
}

int Governor::GetRingingTimeInFrames() {
  return 0;
}

float Governor::GetGovernorMultiplier() {
  if (volume_ >= onset_volume_) {
    return clamp_multiplier_;
  }
  return 1.0;
}

void Governor::SetSlewTimeMsForTest(int slew_time_ms) {
  slew_volume_.SetMaxSlewTimeMs(slew_time_ms);
}

}  // namespace media
}  // namespace chromecast
