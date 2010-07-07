// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_handler.h"

#include <math.h>

#include "base/logging.h"
#include "chrome/browser/chromeos/pulse_audio_mixer.h"

namespace chromeos {

namespace {

const double kSilenceDb = -200.0;
const double kMinVolumeDb = -90.0;
// Choosing 6.0dB here instead of 0dB to give user chance to amplify audio some
// in case sounds or their setup is too quiet for them.
const double kMaxVolumeDb = 6.0;
// A value of less than one adjusts quieter volumes in larger steps (giving
// finer resolution in the higher volumes).
const double kVolumeBias = 0.5;

}  // namespace

// This class will set volume using PulseAudio to adjust volume and mute, and
// handles the volume level logic.

// TODO(davej): Serialize volume/mute for next startup?
// TODO(davej): Should we try to regain a connection if for some reason all was
// initialized fine, but later IsValid() returned false?  Maybe N retries?

double AudioHandler::GetVolumePercent() const {
  if (!SanityCheck())
    return 0;

  return VolumeDbToPercent(mixer_->GetVolumeDb());
}

// Set volume using our internal 0-100% range.  Notice 0% is a special case of
// silence, so we set the mixer volume to kSilenceDb instead of kMinVolumeDb.
void AudioHandler::SetVolumePercent(double volume_percent) {
  if (!SanityCheck())
    return;
  DCHECK(volume_percent >= 0.0);

  double vol_db;
  if (volume_percent <= 0)
    vol_db = kSilenceDb;
  else
    vol_db = PercentToVolumeDb(volume_percent);

  mixer_->SetVolumeDb(vol_db);
}

void AudioHandler::AdjustVolumeByPercent(double adjust_by_percent) {
  if (!SanityCheck())
    return;

  DLOG(INFO) << "Adjusting Volume by " << adjust_by_percent << " percent";

  double volume = mixer_->GetVolumeDb();
  double pct = VolumeDbToPercent(volume);

  if (pct < 0)
    pct = 0;
  pct = pct + adjust_by_percent;
  if (pct > 100.0)
    pct = 100.0;

  double new_volume;
  if (pct <= 0.1)
    new_volume = kSilenceDb;
  else
    new_volume = PercentToVolumeDb(pct);

  if (new_volume != volume)
    mixer_->SetVolumeDb(new_volume);
}

bool AudioHandler::IsMute() const {
  if (!SanityCheck())
    return false;

  return mixer_->IsMute();
}

void AudioHandler::SetMute(bool do_mute) {
  if (!SanityCheck())
    return;

  DLOG(INFO) << "Setting Mute to " << do_mute;

  mixer_->SetMute(do_mute);
}

void AudioHandler::OnMixerInitialized(bool success) {
  connected_ = success;
  DLOG(INFO) << "OnMixerInitialized, success = " << success;
}

AudioHandler::AudioHandler()
    : connected_(false) {
  mixer_.reset(new PulseAudioMixer());
  if (!mixer_->Init(NewCallback(this, &AudioHandler::OnMixerInitialized))) {
    LOG(ERROR) << "Unable to connect to PulseAudio";
  }
}

AudioHandler::~AudioHandler() {
};

inline bool AudioHandler::SanityCheck() const {
  if (!mixer_->IsValid()) {
    if (connected_) {
      // Something happened and the mixer is no longer valid after having been
      // initialized earlier.
      // TODO(davej): Try to reconnect now?
      connected_ = false;
      LOG(ERROR) << "Lost connection to PulseAudio";
    } else {
      LOG(ERROR) << "Mixer not valid";
    }
    return false;
  }
  return true;
}

// VolumeDbToPercent() and PercentToVolumeDb() conversion functions allow us
// complete control over how the 0 to 100% range is mapped to actual loudness.
// Volume range is from kMinVolumeDb at just above 0% to kMaxVolumeDb at 100%
// with a special case at 0% which maps to kSilenceDb.
//
// The mapping is confined to these two functions to make it easy to adjust and
// have everything else just work.  The range is biased to give finer resolution
// in the higher volumes if kVolumeBias is less than 1.0.

// static
double AudioHandler::VolumeDbToPercent(double volume_db) {
  if (volume_db < kMinVolumeDb)
    return 0;
  return 100.0 * pow((volume_db - kMinVolumeDb) /
      (kMaxVolumeDb - kMinVolumeDb), 1/kVolumeBias);
}

// static
double AudioHandler::PercentToVolumeDb(double volume_percent) {
  return pow(volume_percent / 100.0, kVolumeBias) *
      (kMaxVolumeDb - kMinVolumeDb) + kMinVolumeDb;
}

}  // namespace chromeos
