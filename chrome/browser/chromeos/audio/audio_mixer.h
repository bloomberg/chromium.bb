// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_

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

  // Mutes or unmutes the device, possibly asynchronously.  The caller must
  // first verify that the mute state is not locked by calling IsMuteLocked.
  virtual void SetMuted(bool mute) = 0;

  // Is the device's mute state currently locked?
  virtual bool IsMuteLocked() = 0;

  // Locks the mute state of the device to whatever state it currently is in.
  // Call SetMuteLocked with false to allow changing the mute state again.
  virtual void SetMuteLocked(bool locked) = 0;

  // Is the capture device currently muted?
  virtual bool IsCaptureMuted() = 0;

  // Mutes or unmutes the capture device, possible asynchronously.  The caller
  // must first verify that the mute state is not locked by calling
  // IsCaptureMuteLocked.
  virtual void SetCaptureMuted(bool mute) = 0;

  // Is the capture device's mute state currently locked?
  virtual bool IsCaptureMuteLocked() = 0;

  // Locks the capture mute state of the device.
  virtual void SetCaptureMuteLocked(bool locked) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioMixer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_MIXER_H_
