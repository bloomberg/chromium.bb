// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_handler.h"

#include <math.h>

#include "base/logging.h"
#include "base/singleton.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/audio_mixer_alsa.h"
#include "chrome/browser/chromeos/audio_mixer_pulse.h"

namespace chromeos {

namespace {

const double kMinVolumeDb = -90.0;
// Choosing 6.0dB here instead of 0dB to give user chance to amplify audio some
// in case sounds or their setup is too quiet for them.
const double kMaxVolumeDb = 6.0;
// A value of less than one adjusts quieter volumes in larger steps (giving
// finer resolution in the higher volumes).
const double kVolumeBias = 0.5;
// If a connection is lost, we try again this many times
const int kMaxReconnectTries = 4;

}  // namespace

// chromeos:  This class will set the volume using ALSA to adjust volume and
// mute, and handle the volume level logic.  PulseAudio is no longer called
// and support can be removed once there is no need to go back to using PA.

double AudioHandler::GetVolumePercent() {
  if (!VerifyMixerConnection())
    return 0;

  return VolumeDbToPercent(mixer_->GetVolumeDb());
}

// Set volume using our internal 0-100% range.  Notice 0% is a special case of
// silence, so we set the mixer volume to kSilenceDb instead of min_volume_db_.
void AudioHandler::SetVolumePercent(double volume_percent) {
  if (!VerifyMixerConnection())
    return;
  DCHECK(volume_percent >= 0.0);

  double vol_db;
  if (volume_percent <= 0)
    vol_db = AudioMixer::kSilenceDb;
  else
    vol_db = PercentToVolumeDb(volume_percent);

  mixer_->SetVolumeDb(vol_db);
}

void AudioHandler::AdjustVolumeByPercent(double adjust_by_percent) {
  if (!VerifyMixerConnection())
    return;

  DVLOG(1) << "Adjusting Volume by " << adjust_by_percent << " percent";

  double volume = mixer_->GetVolumeDb();
  double pct = VolumeDbToPercent(volume);

  if (pct < 0)
    pct = 0;
  pct = pct + adjust_by_percent;
  if (pct > 100.0)
    pct = 100.0;

  double new_volume;
  if (pct <= 0.1)
    new_volume = AudioMixer::kSilenceDb;
  else
    new_volume = PercentToVolumeDb(pct);

  if (new_volume != volume)
    mixer_->SetVolumeDb(new_volume);
}

bool AudioHandler::IsMute() {
  if (!VerifyMixerConnection())
    return false;

  return mixer_->IsMute();
}

void AudioHandler::SetMute(bool do_mute) {
  if (!VerifyMixerConnection())
    return;
  DVLOG(1) << "Setting Mute to " << do_mute;
  mixer_->SetMute(do_mute);
}

void AudioHandler::Disconnect() {
  mixer_.reset();
}

bool AudioHandler::TryToConnect(bool async) {
  if (mixer_type_ == MIXER_TYPE_PULSEAUDIO) {
    VLOG(1) << "Trying to connect to PulseAudio";
    mixer_.reset(new AudioMixerPulse());
  } else if (mixer_type_ == MIXER_TYPE_ALSA) {
    VLOG(1) << "Trying to connect to ALSA";
    mixer_.reset(new AudioMixerAlsa());
  } else {
    VLOG(1) << "Cannot find valid volume mixer";
    mixer_.reset();
    return false;
  }

  if (async) {
    mixer_->Init(NewCallback(this, &AudioHandler::OnMixerInitialized));
  } else {
    if (!mixer_->InitSync()) {
      VLOG(1) << "Unable to reconnect to Mixer";
      return false;
    }
  }
  return true;
}

void AudioHandler::UseNextMixer() {
  if (mixer_type_ == MIXER_TYPE_PULSEAUDIO)
    mixer_type_ = MIXER_TYPE_ALSA;
  else
    mixer_type_ = MIXER_TYPE_NONE;
}

static void ClipVolume(double* min_volume, double* max_volume) {
  if (*min_volume < kMinVolumeDb)
    *min_volume = kMinVolumeDb;
  if (*max_volume > kMaxVolumeDb)
    *max_volume = kMaxVolumeDb;
}

void AudioHandler::OnMixerInitialized(bool success) {
  connected_ = success;
  DVLOG(1) << "OnMixerInitialized, success = " << success;

  if (connected_) {
    if (mixer_->GetVolumeLimits(&min_volume_db_, &max_volume_db_)) {
      ClipVolume(&min_volume_db_, &max_volume_db_);
    }
    return;
  }

  VLOG(1) << "Unable to connect to mixer, trying next";
  UseNextMixer();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &AudioHandler::TryToConnect, true));
}

AudioHandler::AudioHandler()
    : connected_(false),
      reconnect_tries_(0),
      max_volume_db_(kMaxVolumeDb),
      min_volume_db_(kMinVolumeDb),
      mixer_type_(MIXER_TYPE_ALSA) {
  // TODO(davej): Only attempting a connection to the ALSA mixer now as a first
  // step in removing pulseaudio.  If all goes well, the other references to
  // the pulseaudio mixer can be removed.

  // Start trying to connect to mixers asynchronously, starting with the current
  // mixer_type_.  If the connection fails, another TryToConnect() for the next
  // mixer will be posted at that time.
  TryToConnect(true);
}

AudioHandler::~AudioHandler() {
  Disconnect();
};

bool AudioHandler::VerifyMixerConnection() {
  if (mixer_ == NULL)
    return false;

  AudioMixer::State mixer_state = mixer_->GetState();
  if (mixer_state == AudioMixer::READY)
    return true;
  if (connected_) {
    // Something happened and the mixer is no longer valid after having been
    // initialized earlier.
    connected_ = false;
    LOG(ERROR) << "Lost connection to mixer";
  } else {
    LOG(ERROR) << "Mixer not valid";
  }

  if ((mixer_state == AudioMixer::INITIALIZING) ||
      (mixer_state == AudioMixer::SHUTTING_DOWN))
    return false;

  if (reconnect_tries_ < kMaxReconnectTries) {
    reconnect_tries_++;
    VLOG(1) << "Re-connecting to mixer attempt " << reconnect_tries_ << "/"
            << kMaxReconnectTries;

    connected_ = TryToConnect(false);

    if (connected_) {
      reconnect_tries_ = 0;
      return true;
    }
    LOG(ERROR) << "Unable to re-connect to mixer";
  }
  return false;
}

// VolumeDbToPercent() and PercentToVolumeDb() conversion functions allow us
// complete control over how the 0 to 100% range is mapped to actual loudness.
// Volume range is from min_volume_db_ at just above 0% to max_volume_db_
// at 100% with a special case at 0% which maps to kSilenceDb.
//
// The mapping is confined to these two functions to make it easy to adjust and
// have everything else just work.  The range is biased to give finer resolution
// in the higher volumes if kVolumeBias is less than 1.0.

// static
double AudioHandler::VolumeDbToPercent(double volume_db) const {
  if (volume_db < min_volume_db_)
    return 0;
  return 100.0 * pow((volume_db - min_volume_db_) /
      (max_volume_db_ - min_volume_db_), 1/kVolumeBias);
}

// static
double AudioHandler::PercentToVolumeDb(double volume_percent) const {
  return pow(volume_percent / 100.0, kVolumeBias) *
      (max_volume_db_ - min_volume_db_) + min_volume_db_;
}

// static
AudioHandler* AudioHandler::GetInstance() {
  return Singleton<AudioHandler>::get();
}

}  // namespace chromeos
