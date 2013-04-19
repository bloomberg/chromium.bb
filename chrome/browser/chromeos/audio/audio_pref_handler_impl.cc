// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_pref_handler_impl.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Default value for the volume pref, as a percent in the range [0.0, 100.0].
const double kDefaultVolumePercent = 75.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

}  // namespace

double AudioPrefHandlerImpl::GetOutputVolumeValue() {
  return local_state_->GetDouble(prefs::kAudioVolumePercent);
}

void AudioPrefHandlerImpl::SetOutputVolumeValue(double volume_percent) {
  local_state_->SetDouble(prefs::kAudioVolumePercent, volume_percent);
}

bool AudioPrefHandlerImpl::GetOutputMuteValue() {
  return (local_state_->GetInteger(prefs::kAudioMute) == kPrefMuteOn);
}

void AudioPrefHandlerImpl::SetOutputMuteValue(bool mute) {
  local_state_->SetInteger(prefs::kAudioMute,
                           mute ? kPrefMuteOn : kPrefMuteOff);
}

bool AudioPrefHandlerImpl::GetAudioCaptureAllowedValue() {
  return local_state_->GetBoolean(::prefs::kAudioCaptureAllowed);
}

bool AudioPrefHandlerImpl::GetAudioOutputAllowedValue() {
  return local_state_->GetBoolean(prefs::kAudioOutputAllowed);
}

void AudioPrefHandlerImpl::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.AddObserver(observer);
}

void AudioPrefHandlerImpl::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.RemoveObserver(observer);
}

AudioPrefHandlerImpl::AudioPrefHandlerImpl(PrefService* local_state)
    : local_state_(local_state) {
  InitializePrefObservers();
}

AudioPrefHandlerImpl::~AudioPrefHandlerImpl() {
};

void AudioPrefHandlerImpl::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::Closure callback =
      base::Bind(&AudioPrefHandlerImpl::NotifyAudioPolicyChange,
                 base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
  pref_change_registrar_.Add(::prefs::kAudioCaptureAllowed, callback);
}

void AudioPrefHandlerImpl::NotifyAudioPolicyChange() {
  FOR_EACH_OBSERVER(AudioPrefObserver,
                    observers_,
                    OnAudioPolicyPrefChanged());
}

// static
void AudioPrefHandlerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
  // Register the prefs backing the audio muting policies.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);
  // This pref has moved to the media subsystem but we should verify it is there
  // before we use it.
  registry->RegisterBooleanPref(::prefs::kAudioCaptureAllowed, true);
}

// static
AudioPrefHandler* AudioPrefHandler::Create(PrefService* local_state) {
  return new AudioPrefHandlerImpl(local_state);
}

}  // namespace chromeos
