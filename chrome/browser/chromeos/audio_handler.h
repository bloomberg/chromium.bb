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
  static void Initialize();
  static void Shutdown();
  // GetInstance returns NULL if not initialized or if already shutdown.
  static AudioHandler* GetInstance();

  // Is the mixer initialized?
  // TODO(derat): All of the volume-percent methods will produce "interesting"
  // results before the mixer is initialized, since the driver's volume range
  // isn't known at that point.  This could be avoided if AudioMixer objects
  // instead took percentages and did their own conversions to decibels.
  bool IsInitialized();

  // Gets volume level in our internal 0-100% range, 0 being pure silence.
  double GetVolumePercent();

  // Sets volume level from 0-100%.
  void SetVolumePercent(double volume_percent);

  // Adjusts volume up (positive percentage) or down (negative percentage).
  void AdjustVolumeByPercent(double adjust_by_percent);

  // Is the volume currently muted?
  bool IsMuted();

  // Mutes or unmutes all audio.
  void SetMuted(bool do_mute);

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and constructor/destructor private as recommended for Singletons.
  friend struct DefaultSingletonTraits<AudioHandler>;

  AudioHandler();
  virtual ~AudioHandler();

  // Conversion between our internal scaling (0-100%) and decibels.
  double VolumeDbToPercent(double volume_db) const;
  double PercentToVolumeDb(double volume_percent) const;

  scoped_ptr<AudioMixer> mixer_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

}  // namespace chromeos

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::AudioHandler);

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_HANDLER_H_
