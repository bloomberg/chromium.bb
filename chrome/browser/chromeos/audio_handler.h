// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_

#include "base/scoped_ptr.h"
#include "base/singleton.h"

namespace chromeos {

class PulseAudioMixer;

class AudioHandler {
 public:
  static AudioHandler* instance() {
    return Singleton<AudioHandler>::get();
  }

  // Get current volume level in terms of decibels (dB), silence will return
  // -200 dB.  Returns default of -200.0 on error.  This function is designed
  // to block until the volume is retrieved or fails.  Blocking call.
  double GetVolumeDb() const;

  // Blocking call.
  // TODO(davej): Verify this becomes non-blocking after underlying calls are
  // made non-blocking.
  void SetVolumeDb(double volume_db);

  // Get volume level in our internal 0-100% range, 0 being pure silence.
  // Volume may go above 100% if another process changes PulseAudio's volume.
  // Returns default of 0 on error.  This function will block until the volume
  // is retrieved or fails.  Blocking call.
  double GetVolumePercent() const;

  // Set volume level from 0-100%.  Volumes above 100% are OK and boost volume,
  // although clipping will occur more at higher volumes.  Volume gets quieter
  // as the percentage gets lower, and then switches to silence at 0%.
  // Blocking call.
  // TODO(davej): Verify this becomes non-blocking after underlying calls are
  // made non-blocking.
  void SetVolumePercent(double volume_percent);

  // Adust volume up (positive percentage) or down (negative percentage),
  // capping at 100%.  Call GetVolumePercent() afterwards to get the new level.
  // Blocking call.
  // TODO(davej): Verify this becomes non-blocking after underlying calls are
  // made non-blocking.
  void AdjustVolumeByPercent(double adjust_by_percent);

  // Just returns true if mute, false if not or an error occurred.  This call
  // will block until the mute state is retrieved or fails.  Blocking call.
  bool IsMute() const;

  // Mutes all audio.  Non-blocking call.
  // TODO(davej): Verify this becomes non-blocking after underlying calls are
  // made non-blocking.
  void SetMute(bool do_mute);

  // Toggle mute.  Use this if you do not need to know the mute state, so it is
  // possible to operate asynchronously.  Blocking call.
  // TODO(davej): Verify this becomes non-blocking after underlying calls are
  // made non-blocking.
  void ToggleMute();

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and constructor/destructor private as recommended for Singletons.
  friend struct DefaultSingletonTraits<AudioHandler>;

  AudioHandler();
  virtual ~AudioHandler();
  inline bool SanityCheck() const;

  // Conversion between our internal scaling (0-100%) and decibels.
  static double VolumeDbToPercent(double volume_db);
  static double PercentToVolumeDb(double volume_percent);

  scoped_ptr<PulseAudioMixer> mixer_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_

