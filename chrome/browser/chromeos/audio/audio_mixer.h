// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_
#pragma once

#include "base/basictypes.h"

namespace chromeos {

class AudioMixer {
 public:
  AudioMixer() {}
  virtual ~AudioMixer() {}

  // Initializes the connection to the device.  This must be called on the UI
  // thread; blocking tasks may take place in the background.  Note that the
  // other methods in this interface can be called before Init().
  virtual void Init() = 0;

  // Returns the most-recently-set volume, in the range [0.0, 100.0].
  virtual double GetVolumePercent() = 0;

  // Sets the volume, possibly asynchronously.
  // |percent| should be in the range [0.0, 100.0].
  virtual void SetVolumePercent(double percent) = 0;

  // Is the device currently muted?
  virtual bool IsMuted() = 0;

  // Mutes or unmutes the device, possibly asynchronously.
  virtual void SetMuted(bool mute) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioMixer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_
