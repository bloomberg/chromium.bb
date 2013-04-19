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
#include "chromeos/audio/audio_pref_handler.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Default value for unmuting, as a percent in the range [0.0, 100.0].
// Used when sound is unmuted, but volume was less than kMuteThresholdPercent.
const double kDefaultUnmuteVolumePercent = 4.0;

// Volume value which should be considered as muted in range [0.0, 100.0].
const double kMuteThresholdPercent = 1.0;

static AudioHandler* g_audio_handler = NULL;

}  // namespace

// static
void AudioHandler::Initialize(
    scoped_refptr<AudioPrefHandler> audio_pref_handler) {
  CHECK(!g_audio_handler);
#if defined(USE_CRAS)
  g_audio_handler = new AudioHandler(new AudioMixerCras(), audio_pref_handler);
#else
  g_audio_handler = new AudioHandler(new AudioMixerAlsa(), audio_pref_handler);
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
void AudioHandler::InitializeForTesting(
    AudioMixer* mixer,
    scoped_refptr<AudioPrefHandler> audio_pref_handler) {
  CHECK(!g_audio_handler);
  g_audio_handler = new AudioHandler(mixer, audio_pref_handler);
}

// static
AudioHandler* AudioHandler::GetInstance() {
  VLOG_IF(1, !g_audio_handler)
      << "AudioHandler::GetInstance() called with NULL global instance.";
  return g_audio_handler;
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
  audio_pref_handler_->SetOutputVolumeValue(volume_percent);
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
    audio_pref_handler_->SetOutputMuteValue(mute);

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

void AudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

AudioHandler::AudioHandler(AudioMixer* mixer,
                           scoped_refptr<AudioPrefHandler> audio_pref_handler)
    : mixer_(mixer),
      volume_percent_(0),
      muted_(false),
      audio_pref_handler_(audio_pref_handler) {
  mixer_->Init();

  DCHECK(audio_pref_handler_.get());
  audio_pref_handler_->AddAudioPrefObserver(this);
  ApplyAudioPolicy();
  // Set initial state so that notifications are not triggered.
  muted_ = audio_pref_handler_->GetOutputMuteValue();
  volume_percent_ = audio_pref_handler->GetOutputVolumeValue();
  SetMuted(muted_);
  SetVolumePercentInternal(volume_percent_);
}

AudioHandler::~AudioHandler() {
  mixer_.reset();
  audio_pref_handler_->RemoveAudioPrefObserver(this);
  audio_pref_handler_ = NULL;
};

void AudioHandler::ApplyAudioPolicy() {
  mixer_->SetMuteLocked(false);
  bool muted = false;
  if (audio_pref_handler_->GetAudioOutputAllowedValue()) {
    muted = audio_pref_handler_->GetOutputMuteValue();
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
  if (audio_pref_handler_->GetAudioCaptureAllowedValue()) {
    mixer_->SetCaptureMuted(false);
  } else {
    mixer_->SetCaptureMuted(true);
    mixer_->SetCaptureMuteLocked(true);
  }
}

}  // namespace chromeos
