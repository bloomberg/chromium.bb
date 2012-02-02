// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

class AudioMixer;

class AudioHandler {
 public:
  class VolumeObserver {
   public:
    virtual void OnVolumeChanged() = 0;
   protected:
    VolumeObserver() {}
    virtual ~VolumeObserver() {}
    DISALLOW_COPY_AND_ASSIGN(VolumeObserver);
  };

  static void Initialize();
  static void Shutdown();
  // GetInstance returns NULL if not initialized or if already shutdown.
  // The mixer may be uninitialized, so use GetInstanceIfInitialized
  // for volume control until the TODO below is resolved.
  static AudioHandler* GetInstance();

  // GetInstanceIfInitialized returns NULL if GetInstance returns NULL or if
  // the mixer has not finished initializing.
  // TODO(derat): All of the volume-percent methods will produce "interesting"
  // results before the mixer is initialized, since the driver's volume range
  // isn't known at that point.  This could be avoided if AudioMixer objects
  // instead took percentages and did their own conversions to decibels.
  static AudioHandler* GetInstanceIfInitialized();

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

  void AddVolumeObserver(VolumeObserver* observer);
  void RemoveVolumeObserver(VolumeObserver* observer);

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and constructor/destructor private as recommended for Singletons.
  friend struct DefaultSingletonTraits<AudioHandler>;

  AudioHandler();
  virtual ~AudioHandler();

  bool IsMixerInitialized();

  // Conversion between our internal scaling (0-100%) and decibels.
  double VolumeDbToPercent(double volume_db) const;
  double PercentToVolumeDb(double volume_percent) const;

  scoped_ptr<AudioMixer> mixer_;

  ObserverList<VolumeObserver> volume_observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
