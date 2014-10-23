// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_

#include "media/base/channel_layout.h"

namespace copresence {

// Audio constants. Currently used from the AudioPlayer/AudioRecorder.
// TODO(rkc): Make these values configurable then remove them from here.
// Number of repetitions of the audio token in one sequence of samples.
extern const int kDefaultRepetitions;

// The default sample rate. We need to ensure that both the recorder and the
// player on _all platforms use the same rate.
extern const float kDefaultSampleRate;
extern const int kDefaultBitsPerSample;

// 18500 for ultrasound, needs to be consistent between platforms.
extern const float kDefaultCarrierFrequency;

// The next two really need to be configurable since they don't need to be
// consistent across platforms, or even playing/recording.
extern const int kDefaultChannels;
extern const media::ChannelLayout kDefaultChannelLayout;

// These constants are used from everywhere.
// Particularly, these are used to index the directive lists in the
// audio manager, so do not change these enums without changing
// audio_directive_list.[h|cc].
enum AudioType {
  AUDIBLE = 0,
  INAUDIBLE = 1,
  BOTH = 2,
  AUDIO_TYPE_UNKNOWN = 3,
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_
