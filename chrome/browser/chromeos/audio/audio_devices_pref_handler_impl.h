// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_

#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/values.h"
#include "chromeos/audio/audio_devices_pref_handler.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Class which implements AudioDevicesPrefHandler interface and register audio
// preferences as well.
class AudioDevicesPrefHandlerImpl : public AudioDevicesPrefHandler {
 public:
  explicit AudioDevicesPrefHandlerImpl(PrefService* local_state);

  // Overridden from AudioDevicesPrefHandler.
  virtual double GetOutputVolumeValue(const AudioDevice* device) OVERRIDE;
  virtual double GetInputGainValue(const AudioDevice* device) OVERRIDE;
  virtual void SetVolumeGainValue(const AudioDevice& device,
                                  double value) OVERRIDE;

  virtual bool GetMuteValue(const AudioDevice& device) OVERRIDE;
  virtual void SetMuteValue(const AudioDevice& device, bool mute_on) OVERRIDE;

  virtual bool GetAudioCaptureAllowedValue() OVERRIDE;
  virtual bool GetAudioOutputAllowedValue() OVERRIDE;

  virtual void AddAudioPrefObserver(AudioPrefObserver* observer) OVERRIDE;
  virtual void RemoveAudioPrefObserver(AudioPrefObserver* observer) OVERRIDE;

  // Registers volume and mute preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  virtual ~AudioDevicesPrefHandlerImpl();

 private:
  // Initializes the observers for the policy prefs.
  void InitializePrefObservers();

  // Update and save methods for the mute preferences for all devices.
  void UpdateDevicesMutePref();
  void SaveDevicesMutePref();

  // Update and save methods for the volume preferences for all devices.
  void UpdateDevicesVolumePref();
  void SaveDevicesVolumePref();

  double GetVolumeGainPrefValue(const AudioDevice& device);
  double GetDeviceDefaultOutputVolume(const AudioDevice& device);

  // Methods to migrate the mute and volume settings for a device from the
  // previous global pref value to the new per device pref value for the
  // current active device. If a previous global setting doesn't exist, we'll
  // use default values of mute = off and volume = 75%.
  void MigrateDeviceMuteSettings(std::string active_device);
  void MigrateDeviceVolumeSettings(std::string active_device);

  // Notifies the AudioPrefObserver for audio policy pref changes.
  void NotifyAudioPolicyChange();

  scoped_ptr<base::DictionaryValue> device_mute_settings_;
  scoped_ptr<base::DictionaryValue> device_volume_settings_;

  PrefService* local_state_;  // not owned
  PrefChangeRegistrar pref_change_registrar_;
  ObserverList<AudioPrefObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
