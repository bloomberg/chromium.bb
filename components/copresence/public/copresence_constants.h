// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_

#include <google/protobuf/repeated_field.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/copresence/tokens.h"
#include "media/base/channel_layout.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class Directive;
class SubscribedMessage;

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

// Shared structs with whispernet. TODO(rkc): These will be removed once we can
// get protobufs working with Nacl. At that point, we'll just pass in
// config_data.proto to the whispernet nacl wrapper directly.

// We will be using fixed types in all these structures since they will be
// stuffed into a string and then read on the other side via a completely
// different toolchain. The struct is in its own namespace to disambiguate it
// from the protobuf structures.

namespace config {

struct AudioDsssParams {
  int64_t include_parity_symbol;
  int64_t use_single_sideband;
  double desired_carrier_frequency;
  int64_t use_crc_16;
  double coder_sample_rate;
  int64_t bits_per_symbol;
  int64_t min_cycles_per_frame;
  int64_t baseband_decimation_factor;
  int64_t upsampling_factor;
  int64_t num_repetitions_to_play;
};

struct AdsrParams {
  int64_t attack_time_millis;
  int64_t decay_time_millis;
  int64_t sustain_time_millis;
  int64_t release_time_millis;
  double sustain_amplitude;
};

struct AudioDtmfParams {
  int64_t include_parity_symbol;
  int64_t use_crc_16;
  double coder_sample_rate;
  int64_t baseband_decimation_factor;
  int64_t frequencies_per_symbol;
  int64_t window_duration_millis;
  AdsrParams adsr_params;
  int64_t num_repetitions_to_play;
};

struct LoggerParam {
  int64_t clear_cached_request_duration_millis;
  int64_t request_buffer_limit;
};

struct AudioParamData {
  LoggerParam logger;
  AudioDsssParams audio_dsss;
  AudioDtmfParams audio_dtmf;
  int64_t recording_channels;
};

}  // namespace config

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


// These callbacks are used from various places in Copresence.

// Generic callback to indicate a boolean success or failure.
using SuccessCallback = base::Callback<void(bool)>;

// Callback to pass around found tokens.
using TokensCallback = base::Callback<void(const std::vector<AudioToken>&)>;

// Callback to receive encoded samples from Whispernet.
// AudioType type: Type of audio encoding - AUDIBLE or INAUDIBLE.
// const std::string& token: The token that we encoded.
// const scoped_refptr<media::AudioBusRefCounted>& samples - Encoded samples.
using SamplesCallback =
    base::Callback<void(AudioType,
                        const std::string&,
                        const scoped_refptr<media::AudioBusRefCounted>&)>;

// Callback to pass a list of directives back to CopresenceState.
using DirectivesCallback = base::Callback<void(const std::vector<Directive>&)>;

// Callback to pass around a list of SubscribedMessages.
using MessagesCallback = base::Callback<void(
    const google::protobuf::RepeatedPtrField<SubscribedMessage>&)>;

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_
