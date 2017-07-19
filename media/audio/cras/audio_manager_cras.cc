// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"

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

// Default input buffer size.
const int kDefaultInputBufferSize = 1024;

const char kBeamformingOnDeviceId[] = "default-beamforming-on";
const char kBeamformingOffDeviceId[] = "default-beamforming-off";

const char kInternalInputVirtualDevice[] = "Built-in mic";
const char kInternalOutputVirtualDevice[] = "Built-in speaker";
const char kHeadphoneLineOutVirtualDevice[] = "Headphone/Line Out";

enum CrosBeamformingDeviceState {
  BEAMFORMING_DEFAULT_ENABLED = 0,
  BEAMFORMING_USER_ENABLED,
  BEAMFORMING_DEFAULT_DISABLED,
  BEAMFORMING_USER_DISABLED,
  BEAMFORMING_STATE_MAX = BEAMFORMING_USER_DISABLED
};

void RecordBeamformingDeviceState(CrosBeamformingDeviceState state) {
  UMA_HISTOGRAM_ENUMERATION("Media.CrosBeamformingDeviceState", state,
                            BEAMFORMING_STATE_MAX + 1);
}

bool IsBeamformingDefaultEnabled() {
  return base::FieldTrialList::FindFullName("ChromebookBeamforming") ==
         "Enabled";
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

// Process |device_list| that two shares the same dev_index by creating a
// virtual device name for them.
void ProcessVirtualDeviceName(AudioDeviceNames* device_names,
                              const chromeos::AudioDeviceList& device_list) {
  DCHECK_EQ(2U, device_list.size());
  if (device_list[0].type == chromeos::AUDIO_TYPE_LINEOUT ||
      device_list[1].type == chromeos::AUDIO_TYPE_LINEOUT) {
    device_names->emplace_back(kHeadphoneLineOutVirtualDevice,
                               base::Uint64ToString(device_list[0].id));
  } else if (device_list[0].type == chromeos::AUDIO_TYPE_INTERNAL_SPEAKER ||
             device_list[1].type == chromeos::AUDIO_TYPE_INTERNAL_SPEAKER) {
    device_names->emplace_back(kInternalOutputVirtualDevice,
                               base::Uint64ToString(device_list[0].id));
  } else {
    DCHECK(device_list[0].type == chromeos::AUDIO_TYPE_INTERNAL_MIC ||
           device_list[1].type == chromeos::AUDIO_TYPE_INTERNAL_MIC);
    device_names->emplace_back(kInternalInputVirtualDevice,
                               base::Uint64ToString(device_list[0].id));
  }
}

}  // namespace

// Adds the beamforming on and off devices to |device_names|.
void AudioManagerCras::AddBeamformingDevices(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  const std::string beamforming_on_name =
      GetLocalizedStringUTF8(BEAMFORMING_ON_DEFAULT_AUDIO_INPUT_DEVICE_NAME);
  const std::string beamforming_off_name =
      GetLocalizedStringUTF8(BEAMFORMING_OFF_DEFAULT_AUDIO_INPUT_DEVICE_NAME);

  if (IsBeamformingDefaultEnabled()) {
    // The first device in the list is expected to have a "default" device ID.
    // Web apps may depend on this behavior.
    beamforming_on_device_id_ = AudioDeviceDescription::kDefaultDeviceId;
    beamforming_off_device_id_ = kBeamformingOffDeviceId;

    // Users in the experiment will have the "beamforming on" device appear
    // first in the list. This causes it to be selected by default.
    device_names->push_back(
        AudioDeviceName(beamforming_on_name, beamforming_on_device_id_));
    device_names->push_back(
        AudioDeviceName(beamforming_off_name, beamforming_off_device_id_));
  } else {
    beamforming_off_device_id_ = AudioDeviceDescription::kDefaultDeviceId;
    beamforming_on_device_id_ = kBeamformingOnDeviceId;

    device_names->push_back(
        AudioDeviceName(beamforming_off_name, beamforming_off_device_id_));
    device_names->push_back(
        AudioDeviceName(beamforming_on_name, beamforming_on_device_id_));
  }
}

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

AudioManagerCras::AudioManagerCras(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      beamforming_on_device_id_(nullptr),
      beamforming_off_device_id_(nullptr) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCras::~AudioManagerCras() = default;

void AudioManagerCras::ShowAudioInputSettings() {
  NOTIMPLEMENTED();
}

void AudioManagerCras::GetAudioDeviceNamesImpl(bool is_input,
                                               AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  // At least two mic positions indicates we have a beamforming capable mic
  // array. Add the virtual beamforming device to the list. When this device is
  // queried through GetInputStreamParameters, provide the cached mic positions.
  if (is_input && mic_positions_.size() > 1)
    AddBeamformingDevices(device_names);
  else
    device_names->push_back(AudioDeviceName::CreateDefault());

  if (base::FeatureList::IsEnabled(features::kEnumerateAudioDevices)) {
    chromeos::AudioDeviceList devices;
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);

    // |dev_idx_map| is a map of dev_index and their audio devices.
    std::map<int, chromeos::AudioDeviceList> dev_idx_map;
    for (const auto& device : devices) {
      if (device.is_input != is_input || !device.is_for_simple_usage())
        continue;

      dev_idx_map[dev_index_of(device.id)].push_back(device);
    }

    for (const auto& item : dev_idx_map) {
      if (1 == item.second.size()) {
        const chromeos::AudioDevice& device = item.second.front();
        device_names->emplace_back(device.display_name,
                                   base::Uint64ToString(device.id));
      } else {
        // Create virtual device name for audio nodes that share the same device
        // index.
        ProcessVirtualDeviceName(device_names, item.second);
      }
    }
  }
}

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  mic_positions_ = ParsePointsFromString(MicPositions());
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
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
  if (chromeos::CrasAudioHandler::Get()->HasKeyboardMic())
    params.set_effects(AudioParameters::KEYBOARD_MIC);

  if (mic_positions_.size() > 1) {
    // We have the mic_positions_ check here because one of the beamforming
    // devices will have been assigned the "default" ID, which could otherwise
    // be confused with the ID in the non-beamforming-capable-device case.
    DCHECK(beamforming_on_device_id_);
    DCHECK(beamforming_off_device_id_);

    if (device_id == beamforming_on_device_id_) {
      params.set_mic_positions(mic_positions_);

      // Record a UMA metric based on the state of the experiment and the
      // selected device. This will tell us i) how common it is for users to
      // manually adjust the beamforming device and ii) how contaminated our
      // metric experiment buckets are.
      if (IsBeamformingDefaultEnabled())
        RecordBeamformingDeviceState(BEAMFORMING_DEFAULT_ENABLED);
      else
        RecordBeamformingDeviceState(BEAMFORMING_USER_ENABLED);
    } else if (device_id == beamforming_off_device_id_) {
      if (!IsBeamformingDefaultEnabled())
        RecordBeamformingDeviceState(BEAMFORMING_DEFAULT_DISABLED);
      else
        RecordBeamformingDeviceState(BEAMFORMING_USER_DISABLED);
    }
  }
  return params;
}

std::string AudioManagerCras::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  if (!base::FeatureList::IsEnabled(features::kEnumerateAudioDevices))
    return "";

  chromeos::AudioDeviceList devices;
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);

  if ((beamforming_on_device_id_ &&
       input_device_id == beamforming_on_device_id_) ||
      (beamforming_off_device_id_ &&
       input_device_id == beamforming_off_device_id_)) {
    // These are special devices derived from the internal mic array, so they
    // should be associated to the internal speaker.
    const chromeos::AudioDevice* internal_speaker =
        audio_handler->GetDeviceByType(chromeos::AUDIO_TYPE_INTERNAL_SPEAKER);
    return internal_speaker ? base::Uint64ToString(internal_speaker->id) : "";
  }

  // At this point, we know we have an ordinary input device, so we look up its
  // device_name, which identifies which hardware device it belongs to.
  uint64_t device_id = 0;
  if (!base::StringToUint64(input_device_id, &device_id))
    return "";
  const chromeos::AudioDevice* input_device =
      audio_handler->GetDeviceFromId(device_id);
  if (!input_device)
    return "";

  const base::StringPiece device_name = input_device->device_name;

  // Now search for an output device with the same device name.
  auto output_device_it = std::find_if(
      devices.begin(), devices.end(),
      [device_name](const chromeos::AudioDevice& device) {
        return !device.is_input && device.device_name == device_name;
      });
  return output_device_it == devices.end()
             ? ""
             : base::Uint64ToString(output_device_it->id);
}

std::string AudioManagerCras::GetDefaultOutputDeviceID() {
  return base::Uint64ToString(
      chromeos::CrasAudioHandler::Get()->GetPrimaryActiveOutputNode());
}

const char* AudioManagerCras::GetName() {
  return "CRAS";
}

AudioOutputStream* AudioManagerCras::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  // Pinning stream is not supported for MakeLinearOutputStream.
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId);
}

AudioOutputStream* AudioManagerCras::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // TODO(dgreid): Open the correct input device for unified IO.
  return MakeOutputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

int AudioManagerCras::GetMinimumOutputBufferSizePerBoard() {
  // On faster boards we can use smaller buffer size for lower latency.
  // On slower boards we should use larger buffer size to prevent underrun.
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (board == "kevin")
    return 768;
  else if (board == "samus")
    return 256;
  return 512;
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = GetMinimumOutputBufferSizePerBoard();
  int bits_per_sample = 16;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    buffer_size =
        std::min(static_cast<int>(limits::kMaxAudioBufferSize),
                 std::max(static_cast<int>(limits::kMinAudioBufferSize),
                          input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, bits_per_sample, buffer_size);
}

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  return new CrasUnifiedStream(params, this, device_id);
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

bool AudioManagerCras::IsDefault(const std::string& device_id, bool is_input) {
  AudioDeviceNames device_names;
  GetAudioDeviceNamesImpl(is_input, &device_names);
  DCHECK(!device_names.empty());
  const AudioDeviceName& device_name = device_names.front();
  return device_name.unique_id == device_id;
}

}  // namespace media
