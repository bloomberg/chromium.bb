// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_devices_pref_handler_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace chromeos {

namespace {

// Default value for the volume pref, as a percent in the range [0.0, 100.0].
const double kDefaultVolumePercent = 75.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

}  // namespace

double AudioDevicesPrefHandlerImpl::GetOutputVolumeValue() {
  if (!CrasAudioHandler::IsInitialized())
    return kDefaultVolumePercent;

  UpdateDevicesVolumePref();
  std::string active_device_id = base::Uint64ToString(
      CrasAudioHandler::Get()->GetActiveOutputNode());
  if (!device_volume_settings_->HasKey(active_device_id))
    MigrateDeviceVolumeSettings(active_device_id);
  double volume = kDefaultVolumePercent;
  device_volume_settings_->GetDouble(active_device_id, &volume);
  return volume;
}

void AudioDevicesPrefHandlerImpl::SetOutputVolumeValue(double volume_percent) {
  std::string active_device_id = base::Uint64ToString(
      CrasAudioHandler::Get()->GetActiveOutputNode());
  if (volume_percent > 100.0)
    volume_percent = 100.0;
  if (volume_percent < 0.0)
    volume_percent = 0.0;
  device_volume_settings_->SetDouble(active_device_id, volume_percent);
  SaveDevicesVolumePref();
}

bool AudioDevicesPrefHandlerImpl::GetOutputMuteValue() {
  if (!CrasAudioHandler::IsInitialized())
    return false;

  UpdateDevicesVolumePref();
  std::string active_device_id = base::Uint64ToString(
      CrasAudioHandler::Get()->GetActiveOutputNode());
  if (!device_mute_settings_->HasKey(active_device_id))
    MigrateDeviceMuteSettings(active_device_id);
  int mute = kPrefMuteOff;
  device_mute_settings_->GetInteger(active_device_id, &mute);
  return (mute == kPrefMuteOn);
}

void AudioDevicesPrefHandlerImpl::SetOutputMuteValue(bool mute) {
  std::string active_device_id = base::Uint64ToString(
      CrasAudioHandler::Get()->GetActiveOutputNode());
  device_mute_settings_->SetBoolean(active_device_id,
                                   mute ? kPrefMuteOn : kPrefMuteOff);
  SaveDevicesVolumePref();
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
    dict_update->Set(it.key(), new base::FundamentalValue(mute));
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
    double volume = kDefaultVolumePercent;
    it.value().GetAsDouble(&volume);
    dict_update->Set(it.key(), new base::FundamentalValue(volume));
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

  // TODO(jennyz,rkc): Move the rest of the preferences registered by
  // AudioPrefHandlerImpl::RegisterPrefs here once we remove the old audio
  // handler code.
}

// static
AudioDevicesPrefHandler* AudioDevicesPrefHandler::Create(
    PrefService* local_state) {
  return new AudioDevicesPrefHandlerImpl(local_state);
}

}  // namespace chromeos
