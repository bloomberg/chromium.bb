// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/audio_manager_pulse.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "media/audio/audio_util.h"
#include "media/audio/linux/audio_manager_linux.h"
#include "media/audio/pulse/pulse_input.h"
#include "media/audio/pulse/pulse_output.h"
#include "media/audio/pulse/pulse_stubs.h"
#include "media/audio/pulse/pulse_util.h"

using media_audio_pulse::kModulePulse;
using media_audio_pulse::InitializeStubs;
using media_audio_pulse::StubPathMap;

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

static const base::FilePath::CharType kPulseLib[] =
    FILE_PATH_LITERAL("libpulse.so.0");

// static
AudioManager* AudioManagerPulse::Create() {
  scoped_ptr<AudioManagerPulse> ret(new AudioManagerPulse());
  if (ret->Init())
    return ret.release();

  DVLOG(1) << "PulseAudio is not available on the OS";
  return NULL;
}

AudioManagerPulse::AudioManagerPulse()
    : input_mainloop_(NULL),
      input_context_(NULL),
      devices_(NULL),
      native_input_sample_rate_(0) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerPulse::~AudioManagerPulse() {
  DestroyPulse();
  Shutdown();
}

// Implementation of AudioManager.
bool AudioManagerPulse::HasAudioOutputDevices() {
  // TODO(xians): implement this function.
  return true;
}

bool AudioManagerPulse::HasAudioInputDevices() {
  // TODO(xians): implement this function.
  return true;
}

void AudioManagerPulse::ShowAudioInputSettings() {
  AudioManagerLinux::ShowLinuxAudioInputSettings();
}

void AudioManagerPulse::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  devices_ = device_names;
  AutoPulseLock auto_lock(input_mainloop_);
  pa_operation* operation = pa_context_get_source_info_list(
      input_context_, DevicesInfoCallback, this);
  WaitForOperationCompletion(input_mainloop_, operation);

  // Append the default device on the top of the list if the list is not empty.
  if (!device_names->empty()) {
    device_names->push_front(
        AudioDeviceName(AudioManagerBase::kDefaultDeviceName,
                        AudioManagerBase::kDefaultDeviceId));
  }
}

AudioOutputStream* AudioManagerPulse::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerPulse::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerPulse::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerPulse::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioOutputStream* AudioManagerPulse::MakeOutputStream(
    const AudioParameters& params) {
  return new PulseAudioOutputStream(params, this);
}

AudioInputStream* AudioManagerPulse::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new PulseAudioInputStream(this, device_id, params,
                                   input_mainloop_, input_context_);
}

AudioParameters AudioManagerPulse::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
  // TODO(xians): figure out the optimized buffer size for the Pulse IO.
  int buffer_size = GetAudioHardwareBufferSize();
  if (input_params.frames_per_buffer() < buffer_size)
    buffer_size = input_params.frames_per_buffer();

  // TODO(dalecurtis): This should include bits per channel and channel layout
  // eventually.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, input_params.channel_layout(),
      input_params.sample_rate(), 16, buffer_size);
}

int AudioManagerPulse::GetNativeSampleRate() {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  pa_operation* operation = pa_context_get_server_info(
      input_context_, SampleRateInfoCallback, this);
  WaitForOperationCompletion(input_mainloop_, operation);

  return native_input_sample_rate_;
}

bool AudioManagerPulse::Init() {
  DCHECK(!input_mainloop_);

  StubPathMap paths;

  // Check if the pulse library is avialbale.
  paths[kModulePulse].push_back(kPulseLib);
  if (!InitializeStubs(paths)) {
    DLOG(WARNING) << "Failed on loading the Pulse library and symbols";
    return false;
  }

  // Create a mainloop API and connect to the default server.
  // The mainloop is the internal asynchronous API event loop.
  input_mainloop_ = pa_threaded_mainloop_new();
  if (!input_mainloop_)
    return false;

  // Start the threaded mainloop.
  if (pa_threaded_mainloop_start(input_mainloop_)) {
    DestroyPulse();
    return false;
  }

  // Lock the event loop object, effectively blocking the event loop thread
  // from processing events. This is necessary.
  AutoPulseLock auto_lock(input_mainloop_);

  pa_mainloop_api* pa_mainloop_api =
      pa_threaded_mainloop_get_api(input_mainloop_);
  input_context_ = pa_context_new(pa_mainloop_api, "Chrome input");
  DCHECK(input_context_) << "Failed to create PA context";
  if (!input_context_) {
    return false;
  }

  pa_context_set_state_callback(input_context_, &pulse::ContextStateCallback,
                                input_mainloop_);
  if (pa_context_connect(input_context_, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
    DLOG(ERROR) << "Failed to connect to the context";
    return false;
  }

  // Wait until |input_context_| is ready.  pa_threaded_mainloop_wait() must be
  // called after pa_context_get_state() in case the context is already ready,
  // otherwise pa_threaded_mainloop_wait() will hang indefinitely.
  while (true) {
    pa_context_state_t context_state = pa_context_get_state(input_context_);
    if (!PA_CONTEXT_IS_GOOD(context_state)) {
      return false;
    }
    if (context_state == PA_CONTEXT_READY)
      break;
    pa_threaded_mainloop_wait(input_mainloop_);
  }

  return true;
}

void AudioManagerPulse::DestroyPulse() {
  if (!input_mainloop_) {
    DCHECK(!input_context_);
    return;
  }

  {
    AutoPulseLock auto_lock(input_mainloop_);
    if (input_context_) {
      // Clear our state callback.
      pa_context_set_state_callback(input_context_, NULL, NULL);
      pa_context_disconnect(input_context_);
      pa_context_unref(input_context_);
      input_context_ = NULL;
    }
  }

  pa_threaded_mainloop_stop(input_mainloop_);
  pa_threaded_mainloop_free(input_mainloop_);
  input_mainloop_ = NULL;
}

void AudioManagerPulse::DevicesInfoCallback(pa_context* context,
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
    manager->devices_->push_back(media::AudioDeviceName(info->description,
                                                        info->name));
  }
}

void AudioManagerPulse::SampleRateInfoCallback(pa_context* context,
                                               const pa_server_info* info,
                                               void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  manager->native_input_sample_rate_ = info->sample_spec.rate;
  pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
}

}  // namespace media
