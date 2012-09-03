// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_handler.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#if defined(USE_CRAS)
#include "chrome/browser/chromeos/audio/audio_mixer_cras.h"
#else
#include "chrome/browser/chromeos/audio/audio_mixer_alsa.h"
#endif
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Default value for the volume pref, as a percent in the range [0.0, 100.0].
const double kDefaultVolumePercent = 75.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

static AudioHandler* g_audio_handler = NULL;

}  // namespace

// static
void AudioHandler::Initialize() {
  CHECK(!g_audio_handler);
#if defined(USE_CRAS)
  g_audio_handler = new AudioHandler(new AudioMixerCras());
#else
  g_audio_handler = new AudioHandler(new AudioMixerAlsa());
#endif
}

// static
void AudioHandler::Shutdown() {
  // We may call Shutdown without calling Initialize, e.g. if we exit early.
  if (g_audio_handler) {
    delete g_audio_handler;
    g_audio_handler = NULL;
  }
}

// static
void AudioHandler::InitializeForTesting(AudioMixer* mixer) {
  CHECK(!g_audio_handler);
  g_audio_handler = new AudioHandler(mixer);
}

// static
AudioHandler* AudioHandler::GetInstance() {
  VLOG_IF(1, !g_audio_handler)
      << "AudioHandler::GetInstance() called with NULL global instance.";
  return g_audio_handler;
}

// static
void AudioHandler::RegisterPrefs(PrefService* local_state) {
  if (!local_state->FindPreference(prefs::kAudioVolumePercent))
    local_state->RegisterDoublePref(prefs::kAudioVolumePercent,
                                    kDefaultVolumePercent,
                                    PrefService::UNSYNCABLE_PREF);
  if (!local_state->FindPreference(prefs::kAudioMute))
    local_state->RegisterIntegerPref(prefs::kAudioMute,
                                     kPrefMuteOff,
                                     PrefService::UNSYNCABLE_PREF);

  // Register the prefs backing the audio muting policies.
  local_state->RegisterBooleanPref(prefs::kAudioOutputAllowed,
                                   true,
                                   PrefService::UNSYNCABLE_PREF);
  local_state->RegisterBooleanPref(prefs::kAudioCaptureAllowed,
                                   true,
                                   PrefService::UNSYNCABLE_PREF);

  // Register the old decibel-based pref so we can clear it.
  // TODO(derat): Remove this after R20: http://crbug.com/112039
  if (!local_state->FindPreference(prefs::kAudioVolumeDb))
    local_state->RegisterDoublePref(prefs::kAudioVolumeDb,
                                    0,
                                    PrefService::UNSYNCABLE_PREF);
  local_state->ClearPref(prefs::kAudioVolumeDb);
  local_state->UnregisterPreference(prefs::kAudioVolumeDb);
}

double AudioHandler::GetVolumePercent() {
  return mixer_->GetVolumePercent();
}

void AudioHandler::SetVolumePercent(double volume_percent) {
  volume_percent = min(max(volume_percent, 0.0), 100.0);
  if (IsMuted() && volume_percent > 0.0)
    SetMuted(false);
  mixer_->SetVolumePercent(volume_percent);
  local_state_->SetDouble(prefs::kAudioVolumePercent, volume_percent);
  FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnVolumeChanged());
}

void AudioHandler::AdjustVolumeByPercent(double adjust_by_percent) {
  SetVolumePercent(mixer_->GetVolumePercent() + adjust_by_percent);
}

bool AudioHandler::IsMuted() {
  return mixer_->IsMuted();
}

void AudioHandler::SetMuted(bool mute) {
  if (!mixer_->IsMuteLocked()) {
    mixer_->SetMuted(mute);
    local_state_->SetInteger(prefs::kAudioMute,
                             mute ? kPrefMuteOn : kPrefMuteOff);
    FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnMuteToggled());
  }
}

bool AudioHandler::IsCaptureMuted() {
  return mixer_->IsCaptureMuted();
}

void AudioHandler::SetCaptureMuted(bool mute) {
  if (!mixer_->IsCaptureMuteLocked())
    mixer_->SetCaptureMuted(mute);
}

void AudioHandler::AddVolumeObserver(VolumeObserver* observer) {
  volume_observers_.AddObserver(observer);
}

void AudioHandler::RemoveVolumeObserver(VolumeObserver* observer) {
  volume_observers_.RemoveObserver(observer);
}

void AudioHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = content::Details<std::string>(details).ptr();
    if (*pref_name == prefs::kAudioOutputAllowed ||
        *pref_name == prefs::kAudioCaptureAllowed) {
      ApplyAudioPolicy();
    }
  } else {
    NOTREACHED() << "Unexpected notification type : " << type;
  }
}

AudioHandler::AudioHandler(AudioMixer* mixer)
    : mixer_(mixer),
      local_state_(g_browser_process->local_state()) {
  InitializePrefObservers();
  ApplyAudioPolicy();
  mixer_->SetVolumePercent(local_state_->GetDouble(prefs::kAudioVolumePercent));
  mixer_->Init();
}

AudioHandler::~AudioHandler() {
  mixer_.reset();
};

void AudioHandler::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, this);
  pref_change_registrar_.Add(prefs::kAudioCaptureAllowed, this);
}

void AudioHandler::ApplyAudioPolicy() {
  mixer_->SetMuteLocked(false);
  if (local_state_->GetBoolean(prefs::kAudioOutputAllowed)) {
    mixer_->SetMuted(
        local_state_->GetInteger(prefs::kAudioMute) == kPrefMuteOn);
  } else {
    mixer_->SetMuted(true);
    mixer_->SetMuteLocked(true);
  }
  FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnMuteToggled());
  mixer_->SetCaptureMuteLocked(false);
  if (local_state_->GetBoolean(prefs::kAudioCaptureAllowed)) {
    mixer_->SetCaptureMuted(false);
  } else {
    mixer_->SetCaptureMuted(true);
    mixer_->SetCaptureMuteLocked(true);
  }
}

}  // namespace chromeos
