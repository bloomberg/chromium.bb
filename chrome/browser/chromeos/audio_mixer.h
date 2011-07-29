// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback_old.h"

namespace chromeos {

class AudioMixer {
 public:
  AudioMixer() {}
  virtual ~AudioMixer() {}

  // Initializes the connection to the device.  This must be called on the UI
  // thread; blocking tasks may take place in the background.  IsInitialized()
  // may be called to check if initialization is done.
  virtual void Init() = 0;

  // Returns true if device initialization is complete.
  virtual bool IsInitialized() = 0;

  // Returns the actual volume range available, according to the mixer.
  // Values will be incorrect if called before initialization is complete.
  virtual void GetVolumeLimits(double* min_volume_db,
                               double* max_volume_db) = 0;

  // Gets the most-recently-set volume, in decibels.
  virtual double GetVolumeDb() = 0;

  // Sets the volume, in decibels.
  // If initialization is still in progress, the value will be applied once
  // initialization is complete.
  virtual void SetVolumeDb(double volume_db) = 0;

  // Gets the most-recently set mute state of the default device (true means
  // muted).
  virtual bool IsMuted() = 0;

  // Sets the mute state of the default device.
  // If initialization is still in progress, the value will be applied once
  // initialization is complete.
  virtual void SetMuted(bool mute) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioMixer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_
