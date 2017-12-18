// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processors/post_processor_benchmark.h"

#include <cmath>
#include <ctime>

#include "base/logging.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {
const float kTestDurationSec = 1.0;
const int kBlockSizeFrames = 256;
const int kNumChannels = 2;

// Creates an interleaved stereo chirp waveform with |frames| frames
// from |start_frequency_left| to |end_frequency_left| for left channel and
// from |start_frequency_right| to |end_frequency_right| for right channel,
// where |start_frequency_x| and |end_frequency_x| are normalized frequencies
// (2 * freq_in_hz / sample_rate) i.e. 0 - DC, 1 - nyquist.
// TODO(montvelishsky) move it to general purpose audio toolset.
std::vector<float> GetStereoChirp(size_t frames,
                                  float start_frequency_left,
                                  float end_frequency_left,
                                  float start_frequency_right,
                                  float end_frequency_right) {
  std::vector<float> chirp(frames * 2);
  float ang_left = 0.0;
  float ang_right = 0.0;
  for (size_t i = 0; i < frames; i += 2) {
    ang_left += start_frequency_left +
                i * (end_frequency_left - start_frequency_left) * M_PI / frames;
    ang_right +=
        start_frequency_right +
        i * (end_frequency_right - start_frequency_right) * M_PI / frames;
    chirp[i] = sin(ang_left);
    chirp[i + 1] = sin(ang_right);
  }

  return chirp;
}

}  // namespace

void AudioProcessorBenchmark(AudioPostProcessor* pp, int sample_rate) {
  int test_size_frames = kTestDurationSec * sample_rate;
  // Make test_size multiple of kBlockSizeFrames and calculate effective
  // duration.
  test_size_frames -= test_size_frames % kBlockSizeFrames;
  float effective_duration = static_cast<float>(test_size_frames) / sample_rate;
  std::vector<float> data_in =
      GetStereoChirp(test_size_frames, 0.0, 1.0, 1.0, 0.0);
  clock_t start_clock = clock();
  for (int i = 0; i < test_size_frames; i += kBlockSizeFrames * kNumChannels) {
    pp->ProcessFrames(&data_in[i], kBlockSizeFrames, 1.0, 0.0);
  }
  clock_t stop_clock = clock();
  LOG(INFO) << "At " << sample_rate
            << " frames per second CPU usage: " << std::defaultfloat
            << 100.0 * (stop_clock - start_clock) /
                   (CLOCKS_PER_SEC * effective_duration)
            << "%";
}

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast
