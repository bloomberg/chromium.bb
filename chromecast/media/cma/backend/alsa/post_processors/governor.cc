// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processors/governor.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/cma/backend/alsa/slew_volume.h"

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
  DCHECK_EQ(channels_, 2);
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

int Governor::ProcessFrames(const std::vector<float*>& data,
                            int frames,
                            float volume) {
  DCHECK_EQ(data.size(), static_cast<size_t>(channels_));

  if (volume != volume_) {
    volume_ = volume;
    slew_volume_.SetVolume(GetGovernorMultiplier());
  }

  for (int c = 0; c < channels_; ++c) {
    DCHECK(data[c]);
    slew_volume_.ProcessFMUL(c != 0 /* repeat_transition */, data[c], frames,
                             data[c]);
  }

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

chromecast::media::AudioPostProcessor* AudioPostProcessorShlib_Create(
    const std::string& config,
    int channels) {
  return static_cast<chromecast::media::AudioPostProcessor*>(
      new chromecast::media::Governor(config, channels));
}
