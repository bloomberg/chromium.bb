// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chromecast/media/cma/backend/post_processor_factory.h"
#include "chromecast/media/cma/backend/post_processors/post_processor_unittest.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {

const char kConfigTemplate[] =
    R"config({"gain_db": %f})config";

const char kLibraryPath[] = "libcast_saturated_gain_1.0.so";

std::string MakeConfigString(float gain_db) {
  return base::StringPrintf(kConfigTemplate, gain_db);
}

}  // namespace

// Default tests from post_processor_test
TEST_P(PostProcessorTest, SaturatedGainDelay) {
  PostProcessorFactory factory;
  std::string config = MakeConfigString(0.0);
  auto pp = factory.CreatePostProcessor(kLibraryPath, config, kNumChannels);
  TestDelay(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, SaturatedGainRinging) {
  PostProcessorFactory factory;
  std::string config = MakeConfigString(0.0);
  auto pp = factory.CreatePostProcessor(kLibraryPath, config, kNumChannels);
  TestRingingTime(pp.get(), sample_rate_);
}

// Also tests clipping (by attempting to set gain way too high).
TEST_P(PostProcessorTest, SaturatedGainPassthrough) {
  PostProcessorFactory factory;
  std::string config = MakeConfigString(100.0);
  auto pp = factory.CreatePostProcessor(kLibraryPath, config, kNumChannels);
  TestPassthrough(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, Gain) {
  const int kNumFrames = 256;
  const int kNumChannels = 2;
  PostProcessorFactory factory;
  std::string config = MakeConfigString(20.0);  // Exactly 10x multiplier.
  auto pp = factory.CreatePostProcessor(kLibraryPath, config, kNumChannels);
  std::vector<float> data = GetSineData(256, 1000, sample_rate_);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] /= 100.0;
  }
  float original_amplitude = SineAmplitude(data, kNumChannels);
  pp->ProcessFrames(data.data(), kNumFrames, 1.0 /* doesn't matter */, -20.0);

  EXPECT_FLOAT_EQ(original_amplitude * 10.0, SineAmplitude(data, kNumChannels))
      << "Expected a gain of 20dB";
}

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast
