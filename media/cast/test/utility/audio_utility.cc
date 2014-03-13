// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "media/cast/test/utility/audio_utility.h"

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"

namespace media {
namespace cast {

const double Pi = 3.14159265358979323846;

TestAudioBusFactory::TestAudioBusFactory(int num_channels,
                                         int sample_rate,
                                         float sine_wave_frequency,
                                         float volume)
    : num_channels_(num_channels),
      sample_rate_(sample_rate),
      volume_(volume),
      source_(num_channels, sine_wave_frequency, sample_rate) {
  CHECK_LT(0, num_channels);
  CHECK_LT(0, sample_rate);
  CHECK_LE(0.0f, volume_);
  CHECK_LE(volume_, 1.0f);
}

TestAudioBusFactory::~TestAudioBusFactory() {}

scoped_ptr<AudioBus> TestAudioBusFactory::NextAudioBus(
    const base::TimeDelta& duration) {
  const int num_samples = static_cast<int>((sample_rate_ * duration) /
                                           base::TimeDelta::FromSeconds(1));
  scoped_ptr<AudioBus> bus(AudioBus::Create(num_channels_, num_samples));
  source_.OnMoreData(bus.get(), AudioBuffersState());
  bus->Scale(volume_);
  return bus.Pass();
}

scoped_ptr<PcmAudioFrame> ToPcmAudioFrame(const AudioBus& audio_bus,
                                          int sample_rate) {
  scoped_ptr<PcmAudioFrame> audio_frame(new PcmAudioFrame());
  audio_frame->channels = audio_bus.channels();
  audio_frame->frequency = sample_rate;
  audio_frame->samples.resize(audio_bus.channels() * audio_bus.frames());
  audio_bus.ToInterleaved(audio_bus.frames(),
                          sizeof(audio_frame->samples.front()),
                          &audio_frame->samples.front());
  return audio_frame.Pass();
}

int CountZeroCrossings(const std::vector<int16>& samples) {
  // The sample values must pass beyond |kAmplitudeThreshold| on the opposite
  // side of zero before a crossing will be counted.
  const int kAmplitudeThreshold = 1000;  // Approx. 3% of max amplitude.

  int count = 0;
  std::vector<int16>::const_iterator i = samples.begin();
  int16 last = 0;
  for (; i != samples.end() && abs(last) < kAmplitudeThreshold; ++i)
    last = *i;
  for (; i != samples.end(); ++i) {
    if (abs(*i) >= kAmplitudeThreshold && (last < 0) != (*i < 0)) {
      ++count;
      last = *i;
    }
  }
  return count;
}

// EncodeTimestamp stores a 16-bit number as frequencies in a sample.
// Our internal code tends to work on 10ms chunks of data, and to
// make sure the decoding always work, I wanted to make sure that the
// encoded value can be decoded from 5ms of sample data, assuming a
// sampling rate of 48Khz, this turns out to be 240 samples.
// Each bit of the timestamp is stored as a frequency, where the
// frequency is bit_number * 200 Hz. We also add a 'sense' tone to
// the output, this tone is 17 * 200 = 3400Hz, and when we decode,
// we can use this tone to make sure that we aren't decoding bogus data.
// Also, we use this tone to scale our expectations in case something
// changed changed the volume of the audio.
//
// Normally, we will encode 480 samples (10ms) of data, but when we
// read it will will scan 240 samples at a time until something that
// can be decoded is found.
//
// The intention is to use these routines to encode the frame number
// that goes with each chunk of audio, so if our frame rate is
// 30Hz, we would encode 48000/30 = 1600 samples of "1", then
// 1600 samples of "2", etc. When we decode this, it is possible
// that we get a chunk of data that is spanning two frame numbers,
// so we gray-code the numbers. Since adjacent gray-coded number
// will only differ in one bit, we should never get numbers out
// of sequence when decoding, at least not by more than one.

const double kBaseFrequency = 200;
const int kSamplingFrequency = 48000;
const size_t kNumBits = 16;
const size_t kSamplesToAnalyze = kSamplingFrequency / kBaseFrequency;
const double kSenseFrequency = kBaseFrequency * (kNumBits + 1);
const double kMinSense = 50000.0;

bool EncodeTimestamp(uint16 timestamp,
                     size_t sample_offset,
                     std::vector<int16>* samples) {
  if (samples->size() < kSamplesToAnalyze) {
    return false;
  }
  // gray-code the number
  timestamp = (timestamp >> 1) ^ timestamp;
  std::vector<double> frequencies;
  for (int i = 0; i < kNumBits; i++) {
    if ((timestamp >> i) & 1) {
      frequencies.push_back(kBaseFrequency * (i+1));
    }
  }
  // Carrier sense frequency
  frequencies.push_back(kSenseFrequency);
  for (size_t i = 0; i < samples->size(); i++) {
    double ret = 0.0;
    for (size_t f = 0; f < frequencies.size(); f++) {
      ret += sin((i + sample_offset) * Pi * 2.0 * frequencies[f] /
                 kSamplingFrequency);
    }
    (*samples)[i] = ret * 32766 / (kNumBits + 1);
  }
  return true;
}

namespace {
// We use a slow DCT here since this code is only used for testing.
// While an FFT would probably be faster, it wouldn't be a LOT
// faster since we only analyze 17 out of 120 frequencies.
// With an FFT we would verify that none of the higher frequencies
// contain a lot of energy, which would be useful in detecting
// bogus data.
double DecodeOneFrequency(const int16* samples,
                          size_t length,
                          double frequency) {
  double sin_sum = 0.0;
  double cos_sum = 0.0;
  for (size_t i = 0; i < length; i++) {
    sin_sum += samples[i] * sin(i * Pi * 2 * frequency / kSamplingFrequency);
    cos_sum += samples[i] * cos(i * Pi * 2 * frequency / kSamplingFrequency);
  }
  return sqrt(sin_sum * sin_sum + cos_sum * cos_sum);
}
}  // namespace

// When decoding, we first check for sense frequency, then we decode
// each of the bits. Each frequency must have a strength that is similar to
// the sense frequency or to zero, or the decoding fails. If it fails, we
// move head by 60 samples and try again until we run out of samples.
bool DecodeTimestamp(const std::vector<int16>& samples, uint16* timestamp) {
  for (size_t start = 0;
       start + kSamplesToAnalyze <= samples.size();
       start += kSamplesToAnalyze / 4) {
    double sense = DecodeOneFrequency(&samples[start],
                                      kSamplesToAnalyze,
                                      kSenseFrequency);
    if (sense < kMinSense) continue;
    bool success = true;
    uint16 gray_coded = 0;
    for (int bit = 0; success && bit < kNumBits; bit++) {
      double signal_strength = DecodeOneFrequency(
          &samples[start],
          kSamplesToAnalyze,
          kBaseFrequency * (bit + 1));
      if (signal_strength < sense / 4) {
        // Zero bit, no action
      } else if (signal_strength > sense * 0.75 &&
                 signal_strength < sense * 1.25) {
        // One bit
        gray_coded |= 1 << bit;
      } else {
        success = false;
      }
    }
    if (success) {
      // Convert from gray-coded number to binary.
      uint16 mask;
      for (mask = gray_coded >> 1; mask != 0; mask = mask >> 1) {
        gray_coded = gray_coded ^ mask;
      }
      *timestamp = gray_coded;
      return true;
    }
  }
  return false;
}

}  // namespace cast
}  // namespace media
