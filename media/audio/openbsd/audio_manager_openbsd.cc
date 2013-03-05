// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/pulse_output.h"
#endif
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

#include <fcntl.h>

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
static const int kDefaultSampleRate = 48000;

// Implementation of AudioManager.
static bool HasAudioHardware() {
  int fd;
  const char *file;

  if ((file = getenv("AUDIOCTLDEVICE")) == 0 || *file == '\0')
    file = "/dev/audioctl";

  if ((fd = open(file, O_RDONLY)) < 0)
    return false;

  close(fd);
  return true;
}

bool AudioManagerOpenBSD::HasAudioOutputDevices() {
  return HasAudioHardware();
}

bool AudioManagerOpenBSD::HasAudioInputDevices() {
  return HasAudioHardware();
}

AudioParameters AudioManagerOpenBSD::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, 16, kDefaultInputBufferSize);
}

AudioManagerOpenBSD::AudioManagerOpenBSD() {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
  Shutdown();
}

AudioOutputStream* AudioManagerOpenBSD::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format);
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerOpenBSD::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format);
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerOpenBSD::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format);
  NOTIMPLEMENTED();
  return NULL;
}

AudioInputStream* AudioManagerOpenBSD::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format);
  NOTIMPLEMENTED();
  return NULL;
}

AudioParameters AudioManagerOpenBSD::GetPreferredOutputStreamParameters(
    const AudioParameters& input_params) {
  static const int kDefaultOutputBufferSize = 512;

  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = std::min(buffer_size, input_params.frames_per_buffer());
  }

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size);
}


AudioOutputStream* AudioManagerOpenBSD::MakeOutputStream(
    const AudioParameters& params) {
#if defined(USE_PULSEAUDIO)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUsePulseAudio)) {
    return new PulseAudioOutputStream(params, this);
  }
#endif

  NOTIMPLEMENTED();
  return NULL;
}

// TODO(xians): Merge AudioManagerOpenBSD with AudioManagerPulse;
// static
AudioManager* CreateAudioManager() {
  return new AudioManagerOpenBSD();
}

}  // namespace media
