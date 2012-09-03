// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

template <typename T> struct DefaultSingletonTraits;

class PrefChangeRegistrar;
class PrefService;

namespace chromeos {

class AudioMixer;

class AudioHandler : public content::NotificationObserver {
 public:
  class VolumeObserver {
   public:
    virtual void OnVolumeChanged() = 0;
    virtual void OnMuteToggled() = 0;
   protected:
    VolumeObserver() {}
    virtual ~VolumeObserver() {}
    DISALLOW_COPY_AND_ASSIGN(VolumeObserver);
  };

  static void Initialize();
  static void Shutdown();

  // Same as Initialize but using the specified audio mixer.  It takes
  // ownership of |mixer|.
  static void InitializeForTesting(AudioMixer* mixer);

  // GetInstance returns NULL if not initialized or if already shutdown.
  static AudioHandler* GetInstance();

  // Registers volume and mute preferences.
  static void RegisterPrefs(PrefService* local_state);

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

  // Is the capture volume currently muted?
  bool IsCaptureMuted();

  // Mutes or unmutes all capture devices.
  void SetCaptureMuted(bool do_mute);

  void AddVolumeObserver(VolumeObserver* observer);
  void RemoveVolumeObserver(VolumeObserver* observer);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and constructor/destructor private as recommended for Singletons.
  friend struct DefaultSingletonTraits<AudioHandler>;

  // Takes ownership of |mixer|.
  explicit AudioHandler(AudioMixer* mixer);
  virtual ~AudioHandler();

  // Initializes the observers for the policy prefs.
  void InitializePrefObservers();

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  scoped_ptr<AudioMixer> mixer_;

  ObserverList<VolumeObserver> volume_observers_;

  PrefService* local_state_;  // not owned

  PrefChangeRegistrar pref_change_registrar_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_HANDLER_H_
