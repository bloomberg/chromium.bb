// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_
#define MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_

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
  TestAudioBusFactory(int num_channels,
                      int sample_rate,
                      float sine_wave_frequency,
                      float volume);
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

// Encode |timestamp| into the samples pointed to by 'samples' in a way
// that should be decodable even after compressing/decompressing the audio.
// Assumes 48Khz sampling rate and needs at least 240 samples. Returns
// false if |samples| is too small. If more than 240 samples are available,
// then the timestamp will be repeated. |sample_offset| should contain how
// many samples has been encoded so far, so that we can make smooth
// transitions between encoded chunks.
// See audio_utility.cc for details on how the encoding is done.
bool EncodeTimestamp(uint16 timestamp,
                     size_t sample_offset,
                     std::vector<int16>* samples);

// Decode a timestamp encoded with EncodeTimestamp. Returns true if a
// timestamp was found in |samples|.
bool DecodeTimestamp(const std::vector<int16>& samples, uint16* timestamp);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_
