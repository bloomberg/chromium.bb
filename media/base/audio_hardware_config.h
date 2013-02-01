// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_HARDWARE_CONFIG_H_
#define MEDIA_BASE_AUDIO_HARDWARE_CONFIG_H_

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

// Provides thread safe access to the audio hardware configuration.
class MEDIA_EXPORT AudioHardwareConfig {
 public:
  AudioHardwareConfig(int output_buffer_size, int output_sample_rate,
                      int input_sample_rate,
                      ChannelLayout input_channel_layout);
  virtual ~AudioHardwareConfig();

  // Accessors for the currently cached hardware configuration.  Safe to call
  // from any thread.
  int GetOutputBufferSize();
  int GetOutputSampleRate();
  int GetInputSampleRate();
  ChannelLayout GetInputChannelLayout();

  // Allows callers to update the cached values for either input or output.  The
  // values are paired under the assumption that these values will only be set
  // after an input or output device change respectively.  Safe to call from
  // any thread.
  void UpdateInputConfig(int sample_rate, media::ChannelLayout channel_layout);
  void UpdateOutputConfig(int buffer_size, int sample_rate);

 private:
  // Cached values; access is protected by |config_lock_|.
  base::Lock config_lock_;
  int output_buffer_size_;
  int output_sample_rate_;
  int input_sample_rate_;
  ChannelLayout input_channel_layout_;

  DISALLOW_COPY_AND_ASSIGN(AudioHardwareConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_HARDWARE_CONFIG_H_
