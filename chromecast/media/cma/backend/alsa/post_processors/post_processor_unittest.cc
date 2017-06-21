// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processors/post_processor_unittest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.h"

namespace chromecast {
namespace media {

namespace {

const float kEpsilon = std::numeric_limits<float>::epsilon();

}  // namespace

namespace post_processor_test {

void TestDelay(AudioPostProcessor* pp, int sample_rate) {
  EXPECT_TRUE(pp->SetSampleRate(sample_rate));

  const int kNumFrames = GetMaximumFrames(sample_rate);
  const int kSinFreq = 2000;
  std::vector<float> data = GetSineData(kNumFrames, kSinFreq, sample_rate);
  std::vector<float> expected = GetSineData(kNumFrames, kSinFreq, sample_rate);

  int delay_frames = pp->ProcessFrames(data.data(), kNumFrames, 1.0);
  int delayed_frames = 0;

  // PostProcessors that run in dedicated threads may need to delay
  // until they get data processed asyncronously.
  while (delay_frames >= delayed_frames + kNumFrames) {
    delayed_frames += kNumFrames;
    for (int i = 0; i < kNumFrames * kNumChannels; ++i) {
      EXPECT_EQ(0.0f, data[i]) << i;
    }
    std::vector<float> data = GetSineData(kNumFrames, kSinFreq, sample_rate);
    delay_frames = pp->ProcessFrames(data.data(), kNumFrames, 1.0);

    ASSERT_GE(delay_frames, delayed_frames);
  }

  for (int ch = 0; ch < kNumChannels; ++ch) {
    ASSERT_NE(expected[ch], 0.0);

    int nonzero_frame = delay_frames - delayed_frames;
    for (int f = 0; f < nonzero_frame; ++f) {
      EXPECT_EQ(0.0f, data[f * kNumChannels + ch]) << ch << " " << f;
    }
    EXPECT_NE(0.0f, data[nonzero_frame * kNumChannels + ch])
        << ch << " " << nonzero_frame;
  }
}

void TestRingingTime(AudioPostProcessor* pp, int sample_rate) {
  EXPECT_TRUE(pp->SetSampleRate(sample_rate));

  const int kNumFrames = GetMaximumFrames(sample_rate);
  const int kSinFreq = 200;
  int ringing_time_frames = pp->GetRingingTimeInFrames();
  std::vector<float> data;

  // Send a second of data to excite the filter.
  for (int i = 0; i < sample_rate; i += kNumFrames) {
    data = GetSineData(kNumFrames, kSinFreq, sample_rate);
    pp->ProcessFrames(data.data(), kNumFrames, 1.0);
  }
  // Compute the amplitude of the last buffer
  float original_amplitude = SineAmplitude(data, kNumChannels);

  EXPECT_GE(original_amplitude, 0)
      << "Output of nonzero data is 0; cannot test ringing";

  // Feed |ringing_time_frames| of silence.
  if (ringing_time_frames > 0) {
    int frames_remaining = ringing_time_frames;
    int frames_to_process = std::min(ringing_time_frames, kNumFrames);
    while (frames_remaining > 0) {
      frames_to_process = std::min(frames_to_process, frames_remaining);
      data.assign(frames_to_process * kNumChannels, 0);
      pp->ProcessFrames(data.data(), frames_to_process, 1.0);
      frames_remaining -= frames_to_process;
    }
  }

  // Send a little more data and ensure the amplitude is < 1% the original.
  const int probe_frames = 100;
  data.assign(probe_frames * kNumChannels, 0);
  pp->ProcessFrames(data.data(), probe_frames, 1.0);

  EXPECT_LE(SineAmplitude(data, probe_frames * kNumChannels),
            original_amplitude * 0.01);
}

void TestPassthrough(AudioPostProcessor* pp, int sample_rate) {
  EXPECT_TRUE(pp->SetSampleRate(sample_rate));

  const int kNumFrames = GetMaximumFrames(sample_rate);
  const int kSinFreq = 2000;
  std::vector<float> data = GetSineData(kNumFrames, kSinFreq, sample_rate);
  std::vector<float> expected = GetSineData(kNumFrames, kSinFreq, sample_rate);

  int delay_frames = pp->ProcessFrames(data.data(), kNumFrames, 1.0);
  int delayed_frames = 0;

  // PostProcessors that run in dedicated threads may need to delay
  // until they get data processed asyncronously.
  while (delay_frames >= delayed_frames + kNumFrames) {
    delayed_frames += kNumFrames;
    for (int i = 0; i < kNumFrames * kNumChannels; ++i) {
      EXPECT_EQ(0.0f, data[i]) << i;
    }
    std::vector<float> data = GetSineData(kNumFrames, kSinFreq, sample_rate);
    delay_frames = pp->ProcessFrames(data.data(), kNumFrames, 1.0);

    ASSERT_GE(delay_frames, delayed_frames);
  }

  int delay_samples = (delay_frames - delayed_frames) * kNumChannels;
  ASSERT_LE(delay_samples, static_cast<int>(data.size()));

  CheckArraysEqual(expected.data(), data.data() + delay_samples,
                   data.size() - delay_samples);
}

int GetMaximumFrames(int sample_rate) {
  return kMaxAudioWriteTimeMilliseconds * sample_rate / 1000;
}

template <typename T>
void CheckArraysEqual(T* expected, T* actual, size_t size) {
  std::vector<int> differing_values = CompareArray(expected, actual, size);
  if (differing_values.empty()) {
    return;
  }

  size_t size_to_print =
      std::min(static_cast<size_t>(differing_values[0] + 8), size);
  EXPECT_EQ(differing_values.size(), 0u)
      << "Arrays differ at indices "
      << ::testing::PrintToString(differing_values)
      << "\n  Expected: " << ArrayToString(expected, size_to_print)
      << "\n  Actual:   " << ArrayToString(actual, size_to_print);
}

template <typename T>
std::vector<int> CompareArray(T* expected, T* actual, size_t size) {
  std::vector<int> diffs;
  for (size_t i = 0; i < size; ++i) {
    if (std::abs(expected[i] - actual[i]) > kEpsilon) {
      diffs.push_back(i);
    }
  }
  return diffs;
}

template <typename T>
std::string ArrayToString(T* array, size_t size) {
  std::string result;
  for (size_t i = 0; i < size; ++i) {
    result += ::testing::PrintToString(array[i]) + " ";
  }
  return result;
}

template <typename T>
float SineAmplitude(std::vector<T> data, int num_channels) {
  double max_power = 0;
  int frames = data.size() / num_channels;
  for (int ch = 0; ch < kNumChannels; ++ch) {
    double power = 0;
    for (int f = 0; f < frames; ++f) {
      power += std::pow(data[f * num_channels + ch], 2);
    }
    max_power = std::max(max_power, power);
  }
  return std::sqrt(max_power / frames) * sqrt(2);
}

std::vector<float> GetSineData(size_t frames,
                               float frequency,
                               int sample_rate) {
  std::vector<float> sine(frames * kNumChannels);
  for (size_t i = 0; i < frames; ++i) {
    // Offset by a little so that first value is non-zero
    sine[i * 2] =
        sin(static_cast<float>(i + 1) * frequency * 2 * M_PI / sample_rate);
    sine[i * 2 + 1] =
        cos(static_cast<float>(i) * frequency * 2 * M_PI / sample_rate);
  }
  return sine;
}

PostProcessorTest::PostProcessorTest() : sample_rate_(GetParam()) {}
PostProcessorTest::~PostProcessorTest() = default;

INSTANTIATE_TEST_CASE_P(SampleRates,
                        PostProcessorTest,
                        ::testing::Values(44100, 48000));

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast
