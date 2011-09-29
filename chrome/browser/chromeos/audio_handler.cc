// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_handler.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/audio_mixer_alsa.h"
#include "content/browser/browser_thread.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// A value of less than one adjusts quieter volumes in larger steps (giving
// finer resolution in the higher volumes).
const double kVolumeBias = 0.5;

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

bool AudioHandler::IsInitialized() {
  return mixer_->IsInitialized();
}

double AudioHandler::GetVolumePercent() {
  return VolumeDbToPercent(mixer_->GetVolumeDb());
}

void AudioHandler::SetVolumePercent(double volume_percent) {
  volume_percent = min(max(volume_percent, 0.0), 100.0);
  mixer_->SetVolumeDb(PercentToVolumeDb(volume_percent));
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
}

AudioHandler::AudioHandler()
    : mixer_(new AudioMixerAlsa()) {
  mixer_->Init();
}

AudioHandler::~AudioHandler() {
  mixer_.reset();
};

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
