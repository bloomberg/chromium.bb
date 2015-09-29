// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/media_resources.h"

// cras_util.h headers pull in min/max macros...
// TODO(dgreid): Fix headers such that these aren't imported.
#undef min
#undef max

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
const int kDefaultSampleRate = 48000;

// Define bounds for the output buffer size.
const int kMinimumOutputBufferSize = 512;
const int kMaximumOutputBufferSize = 8192;

// Default input buffer size.
const int kDefaultInputBufferSize = 1024;

const char kBeamformingOnUniqueId[] = "beamforming-on";
const char kBeamformingOffUniqueId[] = "beamforming-off";

void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  // Cras will route audio from a proper physical device automatically.
  device_names->push_back(AudioDeviceName(AudioManager::GetDefaultDeviceName(),
                                          AudioManagerBase::kDefaultDeviceId));
}

// Adds the beamforming on and off devices to |device_names|.
void AddBeamformingDevices(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  device_names->push_back(AudioDeviceName(
      GetLocalizedStringUTF8(BEAMFORMING_ON_DEFAULT_AUDIO_INPUT_DEVICE_NAME),
      kBeamformingOnUniqueId));
  device_names->push_back(AudioDeviceName(
      GetLocalizedStringUTF8(BEAMFORMING_OFF_DEFAULT_AUDIO_INPUT_DEVICE_NAME),
      kBeamformingOffUniqueId));
}

// Returns a mic positions string if the machine has a beamforming capable
// internal mic and otherwise an empty string.
std::string MicPositions() {
  // Get the list of devices from CRAS. An internal mic with a non-empty
  // positions field indicates the machine has a beamforming capable mic array.
  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (const auto& device : devices) {
    if (device.type == chromeos::AUDIO_TYPE_INTERNAL_MIC) {
      // There should be only one internal mic device.
      return device.mic_positions;
    }
  }
  return "";
}

}  // namespace

bool AudioManagerCras::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerCras::HasAudioInputDevices() {
  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_input && devices[i].is_for_simple_usage())
      return true;
  }
  return false;
}

AudioManagerCras::AudioManagerCras(AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory), has_keyboard_mic_(false) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCras::~AudioManagerCras() {
  Shutdown();
}

void AudioManagerCras::ShowAudioInputSettings() {
  NOTIMPLEMENTED();
}

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  mic_positions_ = ParsePointsFromString(MicPositions());
  // At least two mic positions indicates we have a beamforming capable mic
  // array. Add the virtual beamforming device to the list. When this device is
  // queried through GetInputStreamParameters, provide the cached mic positions.
  if (mic_positions_.size() > 1)
    AddBeamformingDevices(device_names);
  else
    AddDefaultDevice(device_names);
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  AddDefaultDevice(device_names);
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  int buffer_size = user_buffer_size ?
      user_buffer_size : kDefaultInputBufferSize;

  // TODO(hshi): Fine-tune audio parameters based on |device_id|. The optimal
  // parameters for the loopback stream may differ from the default.
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, kDefaultSampleRate, 16,
                         buffer_size);
  if (has_keyboard_mic_)
    params.set_effects(AudioParameters::KEYBOARD_MIC);
  if (device_id == kBeamformingOnUniqueId)
    params.set_mic_positions(mic_positions_);
  return params;
}

void AudioManagerCras::SetHasKeyboardMic() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  has_keyboard_mic_ = true;
}

AudioOutputStream* AudioManagerCras::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerCras::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // TODO(dgreid): Open the correct input device for unified IO.
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerCras::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kMinimumOutputBufferSize;
  int bits_per_sample = 16;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    buffer_size =
        std::min(kMaximumOutputBufferSize,
                 std::max(buffer_size, input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, bits_per_sample, buffer_size);
}

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params) {
  return new CrasUnifiedStream(params, this);
}

AudioInputStream* AudioManagerCras::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this, device_id);
}

snd_pcm_format_t AudioManagerCras::BitsToFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return SND_PCM_FORMAT_U8;
    case 16:
      return SND_PCM_FORMAT_S16;
    case 24:
      return SND_PCM_FORMAT_S24;
    case 32:
      return SND_PCM_FORMAT_S32;
    default:
      return SND_PCM_FORMAT_UNKNOWN;
  }
}

}  // namespace media
