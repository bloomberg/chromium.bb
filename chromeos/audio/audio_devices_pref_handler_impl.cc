// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/chromeos_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

// Gets the device id string for storing audio preference. The format of
// device string is a string consisting of 2 parts.
// |integer from lower 32 bit of device id| :
// |0(output device) or 1(input device)|
// If an audio device has both integrated input and output devices, the first 2
// parts of the string could be identical, only the last part will differentiate
// them.
std::string GetDeviceIdString(const chromeos::AudioDevice& device) {
  std::string device_id_string =
      base::Uint64ToString(device.stable_device_id &
                           static_cast<uint64_t>(0xffffffff)) +
      " : " + (device.is_input ? "1" : "0");
  // Replace any periods from the device id string with a space, since setting
  // names cannot contain periods.
  std::replace(device_id_string.begin(), device_id_string.end(), '.', ' ');
  return device_id_string;
}

}  // namespace

namespace chromeos {

double AudioDevicesPrefHandlerImpl::GetOutputVolumeValue(
    const AudioDevice* device) {
  if (!device)
    return kDefaultOutputVolumePercent;
  else
    return GetVolumeGainPrefValue(*device);
}

double AudioDevicesPrefHandlerImpl::GetInputGainValue(
    const AudioDevice* device) {
  DCHECK(device);
  return GetVolumeGainPrefValue(*device);
}

void AudioDevicesPrefHandlerImpl::SetVolumeGainValue(
    const AudioDevice& device, double value) {
  device_volume_settings_->SetDouble(GetDeviceIdString(device), value);

  SaveDevicesVolumePref();
}

bool AudioDevicesPrefHandlerImpl::GetMuteValue(const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_mute_settings_->HasKey(device_id_str))
    MigrateDeviceMuteSettings(device_id_str);

  int mute = kPrefMuteOff;
  device_mute_settings_->GetInteger(device_id_str, &mute);

  return (mute == kPrefMuteOn);
}

void AudioDevicesPrefHandlerImpl::SetMuteValue(const AudioDevice& device,
                                               bool mute) {
  device_mute_settings_->SetInteger(GetDeviceIdString(device),
                                    mute ? kPrefMuteOn : kPrefMuteOff);
  SaveDevicesMutePref();
}

AudioDeviceState AudioDevicesPrefHandlerImpl::GetDeviceState(
    const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_state_settings_->HasKey(device_id_str)) {
    device_state_settings_->SetInteger(
        device_id_str, static_cast<int>(AUDIO_STATE_NOT_AVAILABLE));
    SaveDevicesStatePref();
  }
  int state = static_cast<int>(AUDIO_STATE_NOT_AVAILABLE);
  device_state_settings_->GetInteger(device_id_str, &state);
  return (AudioDeviceState)state;
}

void AudioDevicesPrefHandlerImpl::SetDeviceState(const AudioDevice& device,
                                                 AudioDeviceState state) {
  device_state_settings_->SetInteger(GetDeviceIdString(device),
                                     static_cast<int>(state));
  SaveDevicesStatePref();
}

bool AudioDevicesPrefHandlerImpl::GetAudioOutputAllowedValue() {
  return local_state_->GetBoolean(prefs::kAudioOutputAllowed);
}

void AudioDevicesPrefHandlerImpl::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.AddObserver(observer);
}

void AudioDevicesPrefHandlerImpl::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.RemoveObserver(observer);
}

double AudioDevicesPrefHandlerImpl::GetVolumeGainPrefValue(
    const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_volume_settings_->HasKey(device_id_str))
    MigrateDeviceVolumeSettings(device_id_str);

  // TODO(jennyz, rkc): Return a meaningful input gain default value, when
  // cras has added support for normalizing input gain range.
  double value = device.is_input ?
      0.0 : GetDeviceDefaultOutputVolume(device);
  // TODO(rkc): The above code is completely ignored since we 'always' have a
  // default pref value. Fix this. http://crbug.com/442489
  device_volume_settings_->GetDouble(device_id_str, &value);

  return value;
}

double AudioDevicesPrefHandlerImpl::GetDeviceDefaultOutputVolume(
    const AudioDevice& device) {
  if (device.type == AUDIO_TYPE_HDMI)
    return kDefaultHdmiOutputVolumePercent;
  else
    return kDefaultOutputVolumePercent;
}

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : device_mute_settings_(new base::DictionaryValue()),
      device_volume_settings_(new base::DictionaryValue()),
      device_state_settings_(new base::DictionaryValue()),
      local_state_(local_state) {
  InitializePrefObservers();

  LoadDevicesMutePref();
  LoadDevicesVolumePref();
  LoadDevicesStatePref();
}

AudioDevicesPrefHandlerImpl::~AudioDevicesPrefHandlerImpl() {
}

void AudioDevicesPrefHandlerImpl::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::Closure callback =
      base::Bind(&AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange,
                 base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesMutePref() {
  const base::DictionaryValue* mute_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesMute);
  if (mute_prefs)
    device_mute_settings_.reset(mute_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesMutePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesMute);
  dict_update->MergeDictionary(device_mute_settings_.get());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesVolumePref() {
  const base::DictionaryValue* volume_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesVolumePercent);
  if (volume_prefs)
    device_volume_settings_.reset(volume_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesVolumePref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesVolumePercent);
  dict_update->MergeDictionary(device_volume_settings_.get());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesStatePref() {
  const base::DictionaryValue* state_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesState);
  if (state_prefs)
    device_state_settings_.reset(state_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesStatePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesState);
  base::DictionaryValue::Iterator it(*device_state_settings_);
  dict_update->MergeDictionary(device_state_settings_.get());
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceMuteSettings(
    const std::string& active_device) {
  int old_mute = local_state_->GetInteger(prefs::kAudioMute);
  device_mute_settings_->SetInteger(active_device, old_mute);
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceVolumeSettings(
    const std::string& active_device) {
  double old_volume = local_state_->GetDouble(prefs::kAudioVolumePercent);
  device_volume_settings_->SetDouble(active_device, old_volume);
  SaveDevicesVolumePref();
}

void AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange() {
  FOR_EACH_OBSERVER(AudioPrefObserver,
                    observers_,
                    OnAudioPolicyPrefChanged());
}

// static
void AudioDevicesPrefHandlerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAudioDevicesVolumePercent);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesMute);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesState);

  // Register the prefs backing the audio muting policies.
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);

  // Register the legacy audio prefs for migration.
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultOutputVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
}

}  // namespace chromeos
