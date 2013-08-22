// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_devices_pref_handler_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace {

std::string GetDeviceIdString(const chromeos::AudioDevice& device) {
  return device.device_name + " : " +
         base::Uint64ToString(device.id & static_cast<uint64>(0xffffffff));
}

}

namespace chromeos {

double AudioDevicesPrefHandlerImpl::GetVolumeGainValue(
    const AudioDevice& device) {
  UpdateDevicesVolumePref();

  std::string device_id_str = GetDeviceIdString(device);
  if (!device_volume_settings_->HasKey(device_id_str))
    MigrateDeviceVolumeSettings(device_id_str);

  double volume = kDefaultVolumeGainPercent;
  device_volume_settings_->GetDouble(device_id_str, &volume);

  return volume;
}

void AudioDevicesPrefHandlerImpl::SetVolumeGainValue(
    const AudioDevice& device, double value) {
  device_volume_settings_->SetDouble(GetDeviceIdString(device), value);

  SaveDevicesVolumePref();
}

bool AudioDevicesPrefHandlerImpl::GetMuteValue(const AudioDevice& device) {
  UpdateDevicesMutePref();

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


bool AudioDevicesPrefHandlerImpl::GetAudioCaptureAllowedValue() {
  return local_state_->GetBoolean(prefs::kAudioCaptureAllowed);
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

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : device_mute_settings_(new base::DictionaryValue()),
      device_volume_settings_(new base::DictionaryValue()),
      local_state_(local_state) {
  InitializePrefObservers();

  UpdateDevicesMutePref();
  UpdateDevicesVolumePref();
}

AudioDevicesPrefHandlerImpl::~AudioDevicesPrefHandlerImpl() {
};

void AudioDevicesPrefHandlerImpl::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::Closure callback =
      base::Bind(&AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange,
                 base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
  pref_change_registrar_.Add(prefs::kAudioCaptureAllowed, callback);
}

void AudioDevicesPrefHandlerImpl::UpdateDevicesMutePref() {
  const base::DictionaryValue* mute_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesMute);
  if (mute_prefs)
    device_mute_settings_.reset(mute_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesMutePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesMute);
  base::DictionaryValue::Iterator it(*device_mute_settings_);
  while (!it.IsAtEnd()) {
    int mute = kPrefMuteOff;
    it.value().GetAsInteger(&mute);
    dict_update->SetInteger(it.key(), mute);
    it.Advance();
  }
}

void AudioDevicesPrefHandlerImpl::UpdateDevicesVolumePref() {
  const base::DictionaryValue* volume_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesVolumePercent);
  if (volume_prefs)
    device_volume_settings_.reset(volume_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesVolumePref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesVolumePercent);
  base::DictionaryValue::Iterator it(*device_volume_settings_);
  while (!it.IsAtEnd()) {
    double volume = kDefaultVolumeGainPercent;
    it.value().GetAsDouble(&volume);
    dict_update->SetDouble(it.key(), volume);
    it.Advance();
  }
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceMuteSettings(
    std::string active_device) {
  int old_mute = local_state_->GetInteger(prefs::kAudioMute);
  device_mute_settings_->SetInteger(active_device, old_mute);
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceVolumeSettings(
    std::string active_device) {
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

  // Register the prefs backing the audio muting policies.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);
  // This pref has moved to the media subsystem but we should verify it is there
  // before we use it.
  registry->RegisterBooleanPref(::prefs::kAudioCaptureAllowed, true);

  // Register the legacy audio prefs for migration.
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultVolumeGainPercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
}

// static
AudioDevicesPrefHandler* AudioDevicesPrefHandler::Create(
    PrefService* local_state) {
  return new AudioDevicesPrefHandlerImpl(local_state);
}

}  // namespace chromeos
