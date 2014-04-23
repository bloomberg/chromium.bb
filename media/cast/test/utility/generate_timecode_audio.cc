// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/cast/test/utility/audio_utility.h"

const size_t kSamplingFrequency = 48000;

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <fps> <frames> >output.s16le\n", argv[0]);
    exit(1);
  }
  int fps = atoi(argv[1]);
  const uint32 frames = static_cast<uint32>(std::max(0, atoi(argv[2])));
  std::vector<float> samples(kSamplingFrequency / fps);
  size_t num_samples = 0;
  for (uint32 frame_id = 1; frame_id <= frames; frame_id++) {
    CHECK(media::cast::EncodeTimestamp(
        frame_id, num_samples, samples.size(), &samples.front()));
    num_samples += samples.size();
    for (size_t i = 0; i < samples.size(); ++i) {
      const int16 sample_s16 = static_cast<int16>(samples[i] * kint16max);
      putchar(sample_s16 & 0xff);
      putchar(sample_s16 >> 8);
      putchar(sample_s16 & 0xff);
      putchar(sample_s16 >> 8);
    }
  }
}
