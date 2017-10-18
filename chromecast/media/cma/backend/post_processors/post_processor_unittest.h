// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_

#include <vector>

#include "chromecast/public/media/audio_post_processor_shlib.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file contains basic tests for AudioPostProcessors.
// All AudioPostProcessors should run (and pass) the following tests:
//  TestDelay (tests return value from ProcessFrames)
//  TestRingingTime (from GetRingingTimeFrames)
// Additionally, if it is possible to configure the PostProcessor to be a
// passthrough (no-op), then you should also run
//  TestPassthrough (tests data in = data out, accounting for delay).
//
// Usage:
// TEST_P(PostProcessorTest, DelayTest) {
//   std::unique_ptr<AudioPostProcessor> my_post_processor(
//       AudioPostProcessorShlib_Create(my_config, 2));
//   TestDelay(my_post_processor, sample_rate_);
// }
// (Repeat for TestRingingTime, TestPassthrough).
//
// This will run your test with 44100 and 48000 sample rates.
// You can also make your own tests using the provided helper functions.

namespace chromecast {
namespace media {
namespace post_processor_test {

const int kNumChannels = 2;

void TestDelay(AudioPostProcessor* pp, int sample_rate);
void TestRingingTime(AudioPostProcessor* pp, int sample_rate);
void TestPassthrough(AudioPostProcessor* pp, int sample_rate);

// Returns the maximum number of frames a PostProcessor may be asked to handle
// in a single call.
int GetMaximumFrames(int sample_rate);

// Tests that the first |size| elements of |expected| and |actual| are the same.
template <typename T>
void CheckArraysEqual(T* expected, T* actual, size_t size);

// Returns a list of indexes at which |expected| and |actual| differ.
template <typename T>
std::vector<int> CompareArray(T* expected, T* actual, size_t size);

// Print the first |size| elemenents of |array| to a string.
template <typename T>
std::string ArrayToString(T* array, size_t size);

// Compute the amplitude of a sinusoid as power * sqrt(2)
// This is more robust that looking for the maximum value.
float SineAmplitude(std::vector<float> data, int num_channels);

// Return a vector of |frames| frames of |kNumChannels| interleaved data.
// |frequency| is in hz.
// Channel 0 will be sin(n) and channel 1 will be cos(n).
std::vector<float> GetSineData(size_t frames, float frequency, int sample_rate);

class PostProcessorTest : public ::testing::TestWithParam<int> {
 protected:
  PostProcessorTest();
  ~PostProcessorTest() override;

  int sample_rate_;
};

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_
