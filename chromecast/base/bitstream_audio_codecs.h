// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_
#define CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_

#include <string>

namespace chromecast {

constexpr int kBitstreamAudioCodecNone = 0b00000;
constexpr int kBitstreamAudioCodecAc3 = 0b00001;
constexpr int kBitstreamAudioCodecDts = 0b00010;
constexpr int kBitstreamAudioCodecDtsHd = 0b00100;
constexpr int kBitstreamAudioCodecEac3 = 0b01000;
constexpr int kBitstreamAudioCodecPcmSurround = 0b10000;
constexpr int kBitstreamAudioCodecAll = 0b11111;

std::string BitstreamAudioCodecsToString(int codecs);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_
