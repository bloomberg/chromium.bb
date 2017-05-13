// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chromecast/media/cma/backend/alsa/post_processors/governor.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

const char* kConfigTemplate =
    R"config({"onset_volume": %f, "clamp_multiplier": %f})config";

const int kNumChannels = 2;
const float kDefaultClamp = 0.6f;
const int kNumFrames = 100;
const float kFrequency = 1.0f / kNumFrames;
const int kBytesPerSample = sizeof(int32_t);
const int kSampleRate = 44100;

std::string MakeConfigString(float onset_volume, float clamp_multiplier) {
  return base::StringPrintf(kConfigTemplate, onset_volume, clamp_multiplier);
}

// Frequency is in frames (frequency = frequency_in_hz / sample rate)
std::unique_ptr<::media::AudioBus> GetSineData(size_t frames, float frequency) {
  auto data = ::media::AudioBus::Create(kNumChannels, frames);
  std::vector<int32_t> sine(frames * 2);
  for (size_t i = 0; i < frames; ++i) {
    sine[i * 2] = sin(static_cast<float>(i) * frequency * 2 * M_PI) *
                  std::numeric_limits<int32_t>::max();
    sine[i * 2 + 1] = cos(static_cast<float>(i) * frequency * 2 * M_PI) *
                      std::numeric_limits<int32_t>::max();
  }
  data->FromInterleaved(sine.data(), frames, kBytesPerSample);
  return data;
}

void ScaleData(float* data, int frames, float scale) {
  for (int f = 0; f < frames; ++f) {
    data[f] *= scale;
  }
}

void CompareData(const std::vector<float>& expected,
                 const std::vector<float>& actual,
                 int frames) {
  ASSERT_EQ(expected.size(), actual.size());
  for (int f = 0; f < frames; ++f) {
    EXPECT_FLOAT_EQ(expected[f], actual[f]) << "f: " << f;
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
    governor_ = base::MakeUnique<Governor>(config, kNumChannels);
    governor_->SetSlewTimeMsForTest(0);
    governor_->SetSampleRate(kSampleRate);

    auto data_bus = GetSineData(kNumFrames, kFrequency);
    auto expected_bus = GetSineData(kNumFrames, kFrequency);
    data_.resize(kNumFrames * kNumChannels);
    expected_.resize(kNumFrames * kNumChannels);
    data_bus->ToInterleaved<::media::FloatSampleTypeTraits<float>>(
        kNumFrames, data_.data());
    expected_bus->ToInterleaved<::media::FloatSampleTypeTraits<float>>(
        kNumFrames, expected_.data());
  }

  void CompareBuffers() {
    CompareData(expected_, data_, kNumFrames * kNumChannels);
  }

  void ProcessFrames(float volume) {
    EXPECT_EQ(governor_->ProcessFrames(data_.data(), kNumFrames, volume), 0);
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

}  // namespace media
}  // namespace chromecast
