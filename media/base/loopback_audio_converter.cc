// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/loopback_audio_converter.h"

namespace media {

LoopbackAudioConverter::LoopbackAudioConverter(
    const AudioParameters& input_params,
    const AudioParameters& output_params,
    bool disable_fifo)
    : audio_converter_(input_params, output_params, disable_fifo) {}

LoopbackAudioConverter::~LoopbackAudioConverter() {}

double LoopbackAudioConverter::ProvideInput(AudioBus* audio_bus,
                                            base::TimeDelta buffer_delay) {
  audio_converter_.ConvertWithDelay(buffer_delay, audio_bus);
  return 1.0;
}

}  // namespace media
