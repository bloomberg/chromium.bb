// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_ALSA_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_ALSA_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/audio_mixer.h"
#include "chrome/browser/prefs/pref_member.h"

struct _snd_mixer_elem;
struct _snd_mixer;

namespace chromeos {

class AudioMixerAlsa : public AudioMixer {
 public:
  AudioMixerAlsa();
  virtual ~AudioMixerAlsa();

  // Implementation of AudioMixer
  virtual void Init(InitDoneCallback* callback);
  virtual bool InitSync();
  virtual double GetVolumeDb() const;
  virtual bool GetVolumeLimits(double* vol_min, double* vol_max);
  virtual void SetVolumeDb(double vol_db);
  virtual bool IsMute() const;
  virtual void SetMute(bool mute);
  virtual State GetState() const;

  // Registers volume and mute in preferences
  static void RegisterPrefs(PrefService* local_state);

 private:
  // Called to do initialization in background from worker thread.
  void DoInit(InitDoneCallback* callback);

  // Helper functions to get our message loop thread and prefs initialized.
  bool InitThread();
  void InitPrefs();

  // Try to connect to the ALSA mixer through their simple controls interface,
  // and cache mixer handle and mixer elements we'll be using.
  bool InitializeAlsaMixer();
  void FreeAlsaMixer();
  void DoSetVolumeMute(double pref_volume, int pref_mute);

  // Access to PrefMember variables must be done on UI thread.
  void RestoreVolumeMuteOnUIThread();

  // All these internal volume commands must be called with the lock held.
  double DoGetVolumeDb_Locked() const;
  void DoSetVolumeDb_Locked(double vol_db);

  _snd_mixer_elem* FindElementWithName_Locked(_snd_mixer* handle,
                                              const char* element_name) const;

  bool GetElementVolume_Locked(_snd_mixer_elem* elem,
                               double* current_vol) const;

  // Since volume is done in steps, we may not get the exact volume asked for,
  // so actual_vol will contain the true volume that was set.  This information
  // can be used to further refine the volume by adjust a different mixer
  // element.  The rounding_bias is added in before rounding to the nearest
  // volume step (use 0.5 to round to nearest).
  bool SetElementVolume_Locked(_snd_mixer_elem* elem,
                               double new_vol,
                               double* actual_vol,
                               double rounding_bias);

  // In ALSA, the mixer element's 'switch' is turned off to mute.
  bool GetElementMuted_Locked(_snd_mixer_elem* elem) const;
  void SetElementMuted_Locked(_snd_mixer_elem* elem, bool mute);

  // Volume range limits are computed once during InitializeAlsaMixer.
  double min_volume_;
  double max_volume_;

  // Muting is done by setting volume to minimum, so we must save the original.
  // This is the only state information kept in this object.  In some cases,
  // ALSA can report it has a volume switch and we can turn it off, but it has
  // no effect.
  double save_volume_;

  mutable base::Lock mixer_state_lock_;
  mutable State mixer_state_;

  // Cached contexts for use in ALSA calls.
  _snd_mixer* alsa_mixer_;
  _snd_mixer_elem* elem_master_;
  _snd_mixer_elem* elem_pcm_;

  IntegerPrefMember mute_pref_;
  RealPrefMember volume_pref_;

  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioMixerAlsa);
};

}  // namespace chromeos

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::AudioMixerAlsa);

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_ALSA_H_

