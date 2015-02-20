// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CONFIG_H_
#define CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CONFIG_H_

// Shared structs with whispernet. TODO(rkc): These will be removed once we can
// get protobufs working with Nacl. At that point, we'll just pass in
// config_data.proto to the whispernet nacl wrapper directly.

// We will be using fixed types in all these structures since they will be
// stuffed into a string and then read on the other side via a completely
// different toolchain.

struct AudioDsssParams {
  int64_t include_parity_symbol;
  int64_t use_single_sideband;
  double desired_carrier_frequency;
  int64_t use_crc_16;
  double coder_sample_rate;
  double recording_sample_rate;
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
  double recording_sample_rate;
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

#endif  // CHROME_BROWSER_COPRESENCE_CHROME_WHISPERNET_CONFIG_H_
