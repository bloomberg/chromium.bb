// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chromecast/media/cma/backend/post_processors/governor.h"
#include "chromecast/media/cma/backend/post_processors/post_processor_benchmark.h"
#include "chromecast/media/cma/backend/post_processors/post_processor_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {

const char* kConfigTemplate =
    R"config({"onset_volume": %f, "clamp_multiplier": %f})config";

const float kDefaultClamp = 0.6f;
const int kNumFrames = 100;
const int kFrequency = 2000;
const int kSampleRate = 44100;

std::string MakeConfigString(float onset_volume, float clamp_multiplier) {
  return base::StringPrintf(kConfigTemplate, onset_volume, clamp_multiplier);
}

void ScaleData(float* data, int frames, float scale) {
  for (int f = 0; f < frames; ++f) {
    data[f] *= scale;
  }
}

}  // namespace

class GovernorTest : public ::testing::TestWithParam<float> {
 protected:
  GovernorTest() = default;
  ~GovernorTest() = default;
  void SetUp() override {
    clamp_ = kDefaultClamp;
    onset_volume_ = GetParam();
    std::string config = MakeConfigString(onset_volume_, clamp_);
    governor_ = std::make_unique<Governor>(config, kNumChannels);
    governor_->SetSlewTimeMsForTest(0);
    governor_->SetSampleRate(kSampleRate);

    data_ = GetSineData(kNumFrames, kFrequency, kSampleRate);
    expected_ = GetSineData(kNumFrames, kFrequency, kSampleRate);
  }

  void ProcessFrames(float volume) {
    EXPECT_EQ(governor_->ProcessFrames(data_.data(), kNumFrames, volume, 0), 0);
  }

  void CompareBuffers() {
    CheckArraysEqual(expected_.data(), data_.data(), expected_.size());
  }

  float clamp_;
  float onset_volume_;
  std::unique_ptr<Governor> governor_;
  std::vector<float> data_;
  std::vector<float> expected_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GovernorTest);
};

TEST_P(GovernorTest, ZeroVolume) {
  ProcessFrames(0.0f);
  if (onset_volume_ <= 0.0f) {
    ScaleData(expected_.data(), kNumFrames * kNumChannels, clamp_);
  }
  CompareBuffers();
}

TEST_P(GovernorTest, EpsilonBelowOnset) {
  float volume = onset_volume_ - std::numeric_limits<float>::epsilon();
  ProcessFrames(volume);
  CompareBuffers();
}

TEST_P(GovernorTest, EpsilonAboveOnset) {
  float volume = onset_volume_ + std::numeric_limits<float>::epsilon();
  ProcessFrames(volume);
  ScaleData(expected_.data(), kNumFrames * kNumChannels, clamp_);
  CompareBuffers();
}

TEST_P(GovernorTest, MaxVolume) {
  ProcessFrames(1.0);
  if (onset_volume_ <= 1.0) {
    ScaleData(expected_.data(), kNumFrames * kNumChannels, clamp_);
  }
  CompareBuffers();
}

INSTANTIATE_TEST_CASE_P(GovernorClampVolumeTest,
                        GovernorTest,
                        ::testing::Values(0.0f, 0.1f, 0.5f, 0.9f, 1.0f, 1.1f));

// Default tests from post_processor_test
TEST_P(PostProcessorTest, GovernorDelay) {
  std::string config = MakeConfigString(1.0, 1.0);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  TestDelay(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, GovernorRinging) {
  std::string config = MakeConfigString(1.0, 1.0);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  TestRingingTime(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, GovernorPassthrough) {
  std::string config = MakeConfigString(1.0, 1.0);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  TestPassthrough(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, GovernorBenchmark) {
  std::string config = MakeConfigString(1.0, 1.0);
  auto pp = std::make_unique<Governor>(config, kNumChannels);
  AudioProcessorBenchmark(pp.get(), sample_rate_);
}

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast

/*
Benchmark results:
Device: Google Home Max, test audio duration: 1 sec.
Benchmark           Sample Rate    CPU(%)
----------------------------------------------------
GovernorBenchmark   44100          0.0013%
GovernorBenchmark   48000          0.0015%
*/
