// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_handler.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#if defined(USE_CRAS)
#include "chrome/browser/chromeos/audio/audio_mixer_cras.h"
#else
#include "chrome/browser/chromeos/audio/audio_mixer_alsa.h"
#endif
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

// Default value for unmuting, as a percent in the range [0.0, 100.0].
// Used when sound is unmuted, but volume was less than kMuteThresholdPercent.
const double kDefaultUnmuteVolumePercent = 4.0;

// Volume value which should be considered as muted in range [0.0, 100.0].
const double kMuteThresholdPercent = 1.0;

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
void AudioHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
  // Register the prefs backing the audio muting policies.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);
  // This pref has moved to the media subsystem but we should verify it is there
  // before we use it.
  registry->RegisterBooleanPref(prefs::kAudioCaptureAllowed, true);
}

double AudioHandler::GetVolumePercent() {
  return mixer_->GetVolumePercent();
}

void AudioHandler::SetVolumePercent(double volume_percent) {
  volume_percent = min(max(volume_percent, 0.0), 100.0);
  if (volume_percent <= kMuteThresholdPercent)
    volume_percent = 0.0;
  if (IsMuted() && volume_percent > 0.0)
    SetMuted(false);
  if (!IsMuted() && volume_percent == 0.0)
    SetMuted(true);
  SetVolumePercentInternal(volume_percent);
}

void AudioHandler::SetVolumePercentInternal(double volume_percent) {
  mixer_->SetVolumePercent(volume_percent);
  local_state_->SetDouble(prefs::kAudioVolumePercent, volume_percent);
  if (volume_percent != volume_percent_) {
    volume_percent_ = volume_percent;
    FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnVolumeChanged());
  }
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

    if (!mute) {
      if (GetVolumePercent() <= kMuteThresholdPercent) {
        // Avoid the situation when sound has been unmuted, but the volume
        // is set to a very low value, so user still can't hear any sound.
        SetVolumePercentInternal(kDefaultUnmuteVolumePercent);
      }
    }
    if (mute != muted_) {
      muted_ = mute;
      FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnMuteToggled());
    }
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

AudioHandler::AudioHandler(AudioMixer* mixer)
    : mixer_(mixer),
      local_state_(g_browser_process->local_state()),
      volume_percent_(0),
      muted_(false) {
  InitializePrefObservers();
  mixer_->Init();
  ApplyAudioPolicy();
  // Set initial state so that notifications are not triggered.
  muted_ = (local_state_->GetInteger(prefs::kAudioMute) == kPrefMuteOn);
  volume_percent_ = local_state_->GetDouble(prefs::kAudioVolumePercent);
  SetMuted(muted_);
  SetVolumePercentInternal(volume_percent_);
}

AudioHandler::~AudioHandler() {
  mixer_.reset();
};

void AudioHandler::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::Closure callback = base::Bind(&AudioHandler::ApplyAudioPolicy,
                                      base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
  pref_change_registrar_.Add(prefs::kAudioCaptureAllowed, callback);
}

void AudioHandler::ApplyAudioPolicy() {
  mixer_->SetMuteLocked(false);
  bool muted = false;
  if (local_state_->GetBoolean(prefs::kAudioOutputAllowed)) {
    muted = (local_state_->GetInteger(prefs::kAudioMute) == kPrefMuteOn);
    mixer_->SetMuted(muted);
  } else {
    muted = true;
    mixer_->SetMuted(muted);
    mixer_->SetMuteLocked(true);
  }
  if (muted_ != muted) {
    muted_ = muted;
    FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnMuteToggled());
  }
  mixer_->SetCaptureMuteLocked(false);
  if (local_state_->GetBoolean(prefs::kAudioCaptureAllowed)) {
    mixer_->SetCaptureMuted(false);
  } else {
    mixer_->SetCaptureMuted(true);
    mixer_->SetCaptureMuteLocked(true);
  }
}

}  // namespace chromeos
