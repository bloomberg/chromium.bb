// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_handler.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/audio/audio_mixer_alsa.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// A value of less than one adjusts quieter volumes in larger steps (giving
// finer resolution in the higher volumes).
const double kVolumeBias = 0.5;

// Default value assigned to the pref when it's first created, in decibels.
const double kDefaultVolumeDb = -10.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

static AudioHandler* g_audio_handler = NULL;

}  // namespace

// static
void AudioHandler::Initialize() {
  CHECK(!g_audio_handler);
  g_audio_handler = new AudioHandler();
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
AudioHandler* AudioHandler::GetInstance() {
  VLOG_IF(1, !g_audio_handler)
      << "AudioHandler::GetInstance() called with NULL global instance.";
  return g_audio_handler;
}

// static
AudioHandler* AudioHandler::GetInstanceIfInitialized() {
  return g_audio_handler && g_audio_handler->IsMixerInitialized() ?
         g_audio_handler : NULL;
}

// static
void AudioHandler::RegisterPrefs(PrefService* local_state) {
  // TODO(derat): Store audio volume percent instead of decibels.
  if (!local_state->FindPreference(prefs::kAudioVolume))
    local_state->RegisterDoublePref(prefs::kAudioVolume,
                                    kDefaultVolumeDb,
                                    PrefService::UNSYNCABLE_PREF);
  if (!local_state->FindPreference(prefs::kAudioMute))
    local_state->RegisterIntegerPref(prefs::kAudioMute,
                                     kPrefMuteOff,
                                     PrefService::UNSYNCABLE_PREF);
}

double AudioHandler::GetVolumePercent() {
  return VolumeDbToPercent(mixer_->GetVolumeDb());
}

void AudioHandler::SetVolumePercent(double volume_percent) {
  volume_percent = min(max(volume_percent, 0.0), 100.0);
  double volume_db = PercentToVolumeDb(volume_percent);
  mixer_->SetVolumeDb(volume_db);
  prefs_->SetDouble(prefs::kAudioVolume, volume_db);
  FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnVolumeChanged());
}

void AudioHandler::AdjustVolumeByPercent(double adjust_by_percent) {
  const double old_volume_db = mixer_->GetVolumeDb();
  const double old_percent = VolumeDbToPercent(old_volume_db);
  SetVolumePercent(old_percent + adjust_by_percent);
}

bool AudioHandler::IsMuted() {
  return mixer_->IsMuted();
}

void AudioHandler::SetMuted(bool mute) {
  mixer_->SetMuted(mute);
  prefs_->SetInteger(prefs::kAudioMute, mute ? kPrefMuteOn : kPrefMuteOff);
  FOR_EACH_OBSERVER(VolumeObserver, volume_observers_, OnVolumeChanged());
}

void AudioHandler::AddVolumeObserver(VolumeObserver* observer) {
  volume_observers_.AddObserver(observer);
}

void AudioHandler::RemoveVolumeObserver(VolumeObserver* observer) {
  volume_observers_.RemoveObserver(observer);
}

AudioHandler::AudioHandler()
    : mixer_(new AudioMixerAlsa()),
      prefs_(g_browser_process->local_state()) {
  mixer_->SetVolumeDb(prefs_->GetDouble(prefs::kAudioVolume));
  mixer_->SetMuted(prefs_->GetInteger(prefs::kAudioMute) == kPrefMuteOn);
  mixer_->Init();
}

AudioHandler::~AudioHandler() {
  mixer_.reset();
};

bool AudioHandler::IsMixerInitialized() {
  return mixer_->IsInitialized();
}

// VolumeDbToPercent() and PercentToVolumeDb() conversion functions allow us
// complete control over how the 0 to 100% range is mapped to actual loudness.
//
// The mapping is confined to these two functions to make it easy to adjust and
// have everything else just work.  The range is biased to give finer resolution
// in the higher volumes if kVolumeBias is less than 1.0.

double AudioHandler::VolumeDbToPercent(double volume_db) const {
  double min_volume_db, max_volume_db;
  mixer_->GetVolumeLimits(&min_volume_db, &max_volume_db);

  if (volume_db < min_volume_db)
    return 0.0;
  // TODO(derat): Choose a better mapping between percent and decibels.  The
  // bottom twenty-five percent or so is useless on a CR-48's internal speakers;
  // it's all inaudible.
  return 100.0 * pow((volume_db - min_volume_db) /
      (max_volume_db - min_volume_db), 1/kVolumeBias);
}

double AudioHandler::PercentToVolumeDb(double volume_percent) const {
  double min_volume_db, max_volume_db;
  mixer_->GetVolumeLimits(&min_volume_db, &max_volume_db);

  return pow(volume_percent / 100.0, kVolumeBias) *
      (max_volume_db - min_volume_db) + min_volume_db;
}

}  // namespace chromeos
