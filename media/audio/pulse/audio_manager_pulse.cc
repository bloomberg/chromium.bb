// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/audio_manager_pulse.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/pulse/pulse_input.h"
#include "media/audio/pulse/pulse_output.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Define bounds for the output buffer size.
static const int kMinimumOutputBufferSize = 512;
static const int kMaximumOutputBufferSize = 8192;

// Default input buffer size.
static const int kDefaultInputBufferSize = 1024;

AudioManagerPulse::AudioManagerPulse(std::unique_ptr<AudioThread> audio_thread,
                                     AudioLogFactory* audio_log_factory,
                                     pa_threaded_mainloop* pa_mainloop,
                                     pa_context* pa_context)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      input_mainloop_(pa_mainloop),
      input_context_(pa_context),
      devices_(NULL),
      native_input_sample_rate_(0),
      native_channel_count_(0) {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerPulse::~AudioManagerPulse() = default;

void AudioManagerPulse::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();
  // The Pulse objects are the last things to be destroyed since
  // AudioManagerBase::ShutdownOnAudioThread() needs them.
  pulse::DestroyPulse(input_mainloop_, input_context_);
}

bool AudioManagerPulse::HasAudioOutputDevices() {
  AudioDeviceNames devices;
  GetAudioOutputDeviceNames(&devices);
  return !devices.empty();
}

bool AudioManagerPulse::HasAudioInputDevices() {
  AudioDeviceNames devices;
  GetAudioInputDeviceNames(&devices);
  return !devices.empty();
}

void AudioManagerPulse::GetAudioDeviceNames(
    bool input, media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  devices_ = device_names;
  pa_operation* operation = NULL;
  if (input) {
    operation = pa_context_get_source_info_list(
      input_context_, InputDevicesInfoCallback, this);
  } else {
    operation = pa_context_get_sink_info_list(
        input_context_, OutputDevicesInfoCallback, this);
  }
  WaitForOperationCompletion(input_mainloop_, operation);

  // Prepend the default device if the list is not empty.
  if (!device_names->empty())
    device_names->push_front(AudioDeviceName::CreateDefault());
}

void AudioManagerPulse::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(true, device_names);
}

void AudioManagerPulse::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(false, device_names);
}

AudioParameters AudioManagerPulse::GetInputStreamParameters(
    const std::string& device_id) {
  int user_buffer_size = GetUserBufferSize();
  int buffer_size = user_buffer_size ?
      user_buffer_size : kDefaultInputBufferSize;

  // TODO(xians): add support for querying native channel layout for pulse.
  UpdateNativeAudioHardwareInfo();
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, native_input_sample_rate_,
                         buffer_size);
}

const char* AudioManagerPulse::GetName() {
  return "PulseAudio";
}

AudioOutputStream* AudioManagerPulse::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId);
}

AudioOutputStream* AudioManagerPulse::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params, device_id.empty()
                                      ? AudioDeviceDescription::kDefaultDeviceId
                                      : device_id);
}

AudioInputStream* AudioManagerPulse::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerPulse::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerPulse::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  VLOG_IF(0, !output_device_id.empty()) << "Not implemented!";

  int buffer_size = kMinimumOutputBufferSize;

  // Query native parameters where applicable; Pulse does not require these to
  // be respected though, so prefer the input parameters for channel count.
  UpdateNativeAudioHardwareInfo();
  int sample_rate = native_input_sample_rate_;
  ChannelLayout channel_layout = GuessChannelLayout(native_channel_count_);

  if (input_params.IsValid()) {
    // Use the system's output channel count for the DISCRETE layout. This is to
    // avoid a crash due to the lack of support on the multi-channel beyond 8 in
    // the PulseAudio layer.
    if (input_params.channel_layout() != CHANNEL_LAYOUT_DISCRETE)
      channel_layout = input_params.channel_layout();

    buffer_size =
        std::min(kMaximumOutputBufferSize,
                 std::max(buffer_size, input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, buffer_size);
}

AudioOutputStream* AudioManagerPulse::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  DCHECK(!device_id.empty());
  return new PulseAudioOutputStream(params, device_id, this);
}

AudioInputStream* AudioManagerPulse::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new PulseAudioInputStream(this, device_id, params,
                                   input_mainloop_, input_context_);
}

void AudioManagerPulse::UpdateNativeAudioHardwareInfo() {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  pa_operation* operation = pa_context_get_server_info(
      input_context_, AudioHardwareInfoCallback, this);
  WaitForOperationCompletion(input_mainloop_, operation);
}

void AudioManagerPulse::InputDevicesInfoCallback(pa_context* context,
                                                 const pa_source_info* info,
                                                 int error, void *user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (error) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  // Exclude the output devices.
  if (info->monitor_of_sink == PA_INVALID_INDEX) {
    manager->devices_->push_back(AudioDeviceName(info->description,
                                                 info->name));
  }
}

void AudioManagerPulse::OutputDevicesInfoCallback(pa_context* context,
                                                  const pa_sink_info* info,
                                                  int error, void *user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (error) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  manager->devices_->push_back(AudioDeviceName(info->description,
                                               info->name));
}

void AudioManagerPulse::AudioHardwareInfoCallback(pa_context* context,
                                                  const pa_server_info* info,
                                                  void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  manager->native_input_sample_rate_ = info->sample_spec.rate;
  manager->native_channel_count_ = info->sample_spec.channels;
  pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
}

}  // namespace media
