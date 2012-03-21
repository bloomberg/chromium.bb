// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_CRAS_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_CRAS_H_
#pragma once

#include <cras_client.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/audio/audio_mixer.h"

namespace chromeos {

// Simple wrapper for sending volume and mute commands to the audio server on
// ChromeOS.  Interaction happens on a background thread so that initialization
// can poll until the server is started.
class AudioMixerCras : public AudioMixer {
 public:
  AudioMixerCras();
  virtual ~AudioMixerCras();

  // AudioMixer implementation.
  virtual void Init() OVERRIDE;
  virtual double GetVolumePercent() OVERRIDE;
  virtual void SetVolumePercent(double percent) OVERRIDE;
  virtual bool IsMuted() OVERRIDE;
  virtual void SetMuted(bool muted) OVERRIDE;

 private:
  // Tries to connect to CRAS.  On failure, posts a delayed Connect() task to
  // try again.  Failure could occur if the CRAS server isn't running yet.
  void Connect();

  // Updates |client_| for current values of |volume_percent_| and
  // |is_muted_|. No-op if not connected.
  void ApplyState();

  // Interfaces to the audio server.
  struct cras_client *client_;

  // Indicates if we have connected |client_| to the server.
  bool client_connected_;

  // Most recently-requested volume, in percent.  This variable is updated
  // immediately by SetVolumePercent() (post-initialization); the actual mixer
  // volume is updated later on |thread_| by ApplyState().
  double volume_percent_;

  // Most recently-requested muting state.
  bool is_muted_;

  // Is there already a pending call to ApplyState() scheduled on |thread_|?
  bool apply_is_pending_;

  // Background thread used for interacting with CRAS.
  scoped_ptr<base::Thread> thread_;

  // Guards |volume_percent_|, |is_muted_|, and |apply_is_pending_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioMixerCras);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_CRAS_H_
