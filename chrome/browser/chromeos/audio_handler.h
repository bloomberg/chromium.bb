// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

class InProcessBrowserTest;
template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

class AudioMixer;

class AudioHandler {
 public:
  static AudioHandler* GetInstance();

  // Get volume level in our internal 0-100% range, 0 being pure silence.
  // Returns default of 0 on error.  This function will block until the volume
  // is retrieved or fails.  Blocking call.
  double GetVolumePercent();

  // Set volume level from 0-100%.  Volumes above 100% are OK and boost volume,
  // although clipping will occur more at higher volumes.  Volume gets quieter
  // as the percentage gets lower, and then switches to silence at 0%.
  void SetVolumePercent(double volume_percent);

  // Adjust volume up (positive percentage) or down (negative percentage),
  // capping at 100%.  GetVolumePercent() will be accurate after this
  // blocking call.
  void AdjustVolumeByPercent(double adjust_by_percent);

  // Just returns true if mute, false if not or an error occurred.
  // Blocking call.
  bool IsMute();

  // Mutes all audio.  Non-blocking call.
  void SetMute(bool do_mute);

  // Disconnects from mixer.  Called during shutdown.
  void Disconnect();

 private:
  enum MixerType {
    MIXER_TYPE_ALSA = 0,
    MIXER_TYPE_NONE,
  };

  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and constructor/destructor private as recommended for Singletons.
  friend struct DefaultSingletonTraits<AudioHandler>;

  friend class ::InProcessBrowserTest;
  // Disable audio in browser tests. This is a workaround for the bug
  // crosbug.com/17058. Remove this once it's fixed.
  static void Disable();

  // Connect to the current mixer_type_.
  bool TryToConnect(bool async);

  void OnMixerInitialized(bool success);

  AudioHandler();
  virtual ~AudioHandler();
  bool VerifyMixerConnection();

  // Conversion between our internal scaling (0-100%) and decibels.
  double VolumeDbToPercent(double volume_db) const;
  double PercentToVolumeDb(double volume_percent) const;

  scoped_ptr<AudioMixer> mixer_;

  bool connected_;
  int reconnect_tries_;

  // The min and max volume in decibels, limited to the maximum range of the
  // audio system being used.
  double max_volume_db_;
  double min_volume_db_;

  // Which mixer is being used, ALSA or none.
  MixerType mixer_type_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

}  // namespace chromeos

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::AudioHandler);

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_
