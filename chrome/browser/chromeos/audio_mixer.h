// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"

namespace chromeos {

class AudioMixer {
 public:
  // Approximation of pure silence expressed in decibels.
  static const double kSilenceDb = -200.0;

  enum State {
    UNINITIALIZED = 0,
    INITIALIZING,
    READY,
    SHUTTING_DOWN,
    IN_ERROR,
  };

  AudioMixer() {}
  virtual ~AudioMixer() {}

  // Non-Blocking call.  Connect to Mixer, find a default device, then call the
  // callback when complete with success code.
  typedef Callback1<bool>::Type InitDoneCallback;
  virtual void Init(InitDoneCallback* callback) = 0;

  // Call may block.  Mixer will be connected before returning, unless error.
  virtual bool InitSync() = 0;

  // Call may block.  Returns a default of kSilenceDb on error.
  virtual double GetVolumeDb() const = 0;

  // Non-Blocking call.  Returns the actual volume limits possible according
  // to the mixer.  Limits are left unchanged on error
  virtual bool GetVolumeLimits(double* vol_min, double* vol_max) = 0;

  // Non-blocking call.
  virtual void SetVolumeDb(double vol_db) = 0;

  // Call may block.  Gets the mute state of the default device (true == mute).
  // Returns a default of false on error.
  virtual bool IsMute() const = 0;

  // Non-Blocking call.
  virtual void SetMute(bool mute) = 0;

  // Returns READY if we have a valid working connection to the Mixer.
  // This can return IN_ERROR if we lose the connection, even after an original
  // successful init.  Non-blocking call.
  virtual State GetState() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioMixer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_H_

