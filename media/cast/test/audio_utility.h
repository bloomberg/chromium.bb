// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_AUDIO_UTILITY_H_
#define MEDIA_CAST_TEST_AUDIO_UTILITY_H_

#include "media/audio/simple_sources.h"

namespace base {
class TimeDelta;
}

namespace media {
class AudioBus;
}

namespace media {
namespace cast {

struct PcmAudioFrame;

// Produces AudioBuses of varying duration where each successive output contains
// the continuation of a single sine wave.
class TestAudioBusFactory {
 public:
  TestAudioBusFactory(int num_channels, int sample_rate,
                      float sine_wave_frequency, float volume);
  ~TestAudioBusFactory();

  // Creates a new AudioBus of the given |duration|, filled with the next batch
  // of sine wave samples.
  scoped_ptr<AudioBus> NextAudioBus(const base::TimeDelta& duration);

  // A reasonable test tone.
  static const int kMiddleANoteFreq = 440;

 private:
  const int num_channels_;
  const int sample_rate_;
  const float volume_;
  SineWaveAudioSource source_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioBusFactory);
};

// Convenience function to convert an |audio_bus| to its equivalent
// PcmAudioFrame.
// TODO(miu): Remove this once all code has migrated to use AudioBus.  See
// comment in media/cast/cast_config.h.
scoped_ptr<PcmAudioFrame> ToPcmAudioFrame(const AudioBus& audio_bus,
                                          int sample_rate);

// Assuming |samples| contains a single-frequency sine wave (and maybe some
// low-amplitude noise), count the number of times the sine wave crosses
// zero.
int CountZeroCrossings(const std::vector<int16>& samples);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_AUDIO_UTILITY_H_
