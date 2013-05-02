// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_IMPL_H_

#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "chromeos/audio/audio_pref_handler.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// TODO(jennyz,rkc): This class will be removed once we remove the old Audio
// Handler code.
// Class which implements AudioPrefHandler interface and register audio
// preferences as well.
class AudioPrefHandlerImpl : public AudioPrefHandler {
 public:
  explicit AudioPrefHandlerImpl(PrefService* local_state);

  // Overriden from AudioPreHandler.
  virtual double GetOutputVolumeValue() OVERRIDE;
  virtual void SetOutputVolumeValue(double volume_percent) OVERRIDE;
  virtual bool GetOutputMuteValue() OVERRIDE;
  virtual void SetOutputMuteValue(bool mute_on) OVERRIDE;
  virtual bool GetAudioCaptureAllowedValue() OVERRIDE;
  virtual bool GetAudioOutputAllowedValue() OVERRIDE;
  virtual void AddAudioPrefObserver(AudioPrefObserver* observer) OVERRIDE;
  virtual void RemoveAudioPrefObserver(AudioPrefObserver* observer) OVERRIDE;

  // Registers volume and mute preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  virtual ~AudioPrefHandlerImpl();

 private:
  // Initializes the observers for the policy prefs.
  void InitializePrefObservers();

  // Notifies the AudioPrefObserver for audio policy pref changes.
  void NotifyAudioPolicyChange();

  PrefService* local_state_;  // not owned

  PrefChangeRegistrar pref_change_registrar_;

  ObserverList<AudioPrefObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioPrefHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_PREF_HANDLER_IMPL_H_
