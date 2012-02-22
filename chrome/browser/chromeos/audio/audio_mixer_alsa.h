// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_ALSA_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_ALSA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/audio/audio_mixer.h"

struct _snd_mixer_elem;
struct _snd_mixer;

namespace chromeos {

// Simple wrapper around ALSA's mixer functions.
// Interaction with ALSA takes place on a background thread.
class AudioMixerAlsa : public AudioMixer {
 public:
  AudioMixerAlsa();
  virtual ~AudioMixerAlsa();

  // AudioMixer implementation.
  virtual void Init() OVERRIDE;
  virtual double GetVolumePercent() OVERRIDE;
  virtual void SetVolumePercent(double percent) OVERRIDE;
  virtual bool IsMuted() OVERRIDE;
  virtual void SetMuted(bool muted) OVERRIDE;

 private:
  // Tries to connect to ALSA.  On failure, posts a delayed Connect() task to
  // try again.
  void Connect();

  // Helper method called by Connect().  On success, initializes
  // |min_volume_db_|, |max_volume_db_|, |alsa_mixer_|, and |element_*| and
  // returns true.
  bool ConnectInternal();

  // Disconnects from ALSA if currently connected and signals
  // |disconnected_event_|.
  void Disconnect();

  // Updates |alsa_mixer_| for current values of |volume_db_| and |is_muted_|.
  // No-op if not connected.
  void ApplyState();

  // Finds the element named |element_name|.  Returns NULL on failure.
  _snd_mixer_elem* FindElementWithName(_snd_mixer* handle,
                                       const std::string& element_name) const;

  // Queries |element|'s current volume, copying it to |new_volume_db|.
  // Returns true on success.
  bool GetElementVolume(_snd_mixer_elem* element, double* current_volume_db);

  // Sets |element|'s volume.  Since volume is done in steps, we may not get the
  // exact volume asked for.  |rounding_bias| is added in before rounding to the
  // nearest volume step (use 0.5 to round to nearest).
  bool SetElementVolume(_snd_mixer_elem* element,
                        double new_volume_db,
                        double rounding_bias);

  // Mutes or unmutes |element|.
  void SetElementMuted(_snd_mixer_elem* element, bool mute);

  // Converts between percentages (in the range [0.0, 100.0]) and decibels.
  // |lock_| must be held.
  double DbToPercent(double db) const;
  double PercentToDb(double percent) const;

  // Guards |min_volume_db_|, |max_volume_db_|, |volume_db_|, |is_muted_|,
  // |apply_is_pending_|, and |initial_volume_percent_|.
  base::Lock lock_;

  // Volume range limits are computed once in ConnectInternal().
  double min_volume_db_;
  double max_volume_db_;

  // Most recently-requested volume, in decibels.  This variable is updated
  // immediately by SetVolumePercent() (post-initialization); the actual mixer
  // volume is updated later on |thread_| by ApplyState().
  double volume_db_;

  // Most recently-requested muting state.
  bool is_muted_;

  // Is there already a pending call to ApplyState() scheduled on |thread_|?
  bool apply_is_pending_;

  // Before |min_volume_db_| and |max_volume_db_| are fetched from ALSA, we
  // can't convert percentages to decibels.  The default initial volume and any
  // subsequent requests we get early on are stored here and then applied during
  // initialization.  This variable is unused after initialization.
  double initial_volume_percent_;

  // Connection to ALSA.  NULL if not connected.
  _snd_mixer* alsa_mixer_;

  // Master mixer.
  _snd_mixer_elem* master_element_;

  // PCM mixer.  May be NULL if the driver doesn't expose one.
  _snd_mixer_elem* pcm_element_;

  // Signalled after Disconnect() finishes (which is itself invoked by the
  // d'tor).
  base::WaitableEvent disconnected_event_;

  // Background thread used for interacting with ALSA.
  scoped_ptr<base::Thread> thread_;

  // Number of times that we've attempted to connect to ALSA.  Just used to keep
  // us from spamming the logs.
  int num_connection_attempts_;

  DISALLOW_COPY_AND_ASSIGN(AudioMixerAlsa);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_ALSA_H_
