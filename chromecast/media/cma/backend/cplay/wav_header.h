// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CPLAY_WAV_HEADER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CPLAY_WAV_HEADER_H_

#include <stdint.h>

namespace chromecast {
namespace media {

// From http://soundfile.sapp.org/doc/WaveFormat/
struct __attribute__((packed)) WavHeader {
  const char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunk_size;  // size of file - 8
  const char wave[4] = {'W', 'A', 'V', 'E'};
  const char fmt[4] = {'f', 'm', 't', ' '};
  const uint32_t subchunk_size = 16;
  const uint16_t audio_format = 1;  // PCM
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;    // sample_rate * num_channels * bytes per sample
  uint16_t block_align;  // num_channels * bytes per sample
  uint16_t bits_per_sample;
  const char data[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk_2_size;  // bytes in the data

  WavHeader();
  ~WavHeader() = default;

  void SetDataSize(int size_bytes) {
    chunk_size = 36 + size_bytes;
    subchunk_2_size = size_bytes;
  }

  void SetNumChannels(int num_channels_in) {
    num_channels = num_channels_in;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    block_align = num_channels * bits_per_sample / 8;
  }

  void SetSampleRate(int sample_rate_in) {
    sample_rate = sample_rate_in;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
  }

  void SetBitsPerSample(int bits_per_sample_in) {
    bits_per_sample = bits_per_sample_in;
    byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    block_align = num_channels * bits_per_sample / 8;
  }
};

WavHeader::WavHeader() = default;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CPLAY_WAV_HEADER_H_
