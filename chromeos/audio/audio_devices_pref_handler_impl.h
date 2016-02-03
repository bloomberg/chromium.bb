// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/values.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/chromeos_export.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Class which implements AudioDevicesPrefHandler interface and register audio
// preferences as well.
class CHROMEOS_EXPORT AudioDevicesPrefHandlerImpl
    : public AudioDevicesPrefHandler {
 public:
  // |local_state| is the device-wide preference service.
  explicit AudioDevicesPrefHandlerImpl(PrefService* local_state);

  // Overridden from AudioDevicesPrefHandler.
  double GetOutputVolumeValue(const AudioDevice* device) override;
  double GetInputGainValue(const AudioDevice* device) override;
  void SetVolumeGainValue(const AudioDevice& device, double value) override;

  bool GetMuteValue(const AudioDevice& device) override;
  void SetMuteValue(const AudioDevice& device, bool mute_on) override;

  AudioDeviceState GetDeviceState(const AudioDevice& device) override;
  void SetDeviceState(const AudioDevice& device,
                      AudioDeviceState state) override;

  bool GetAudioOutputAllowedValue() override;

  void AddAudioPrefObserver(AudioPrefObserver* observer) override;
  void RemoveAudioPrefObserver(AudioPrefObserver* observer) override;

  // Registers volume and mute preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  ~AudioDevicesPrefHandlerImpl() override;

 private:
  // Initializes the observers for the policy prefs.
  void InitializePrefObservers();

  // Load and save methods for the mute preferences for all devices.
  void LoadDevicesMutePref();
  void SaveDevicesMutePref();

  // Load and save methods for the volume preferences for all devices.
  void LoadDevicesVolumePref();
  void SaveDevicesVolumePref();

  // Load and save methods for the active state for all devices.
  void LoadDevicesStatePref();
  void SaveDevicesStatePref();

  double GetVolumeGainPrefValue(const AudioDevice& device);
  double GetDeviceDefaultOutputVolume(const AudioDevice& device);

  // Methods to migrate the mute and volume settings for a device from the
  // previous global pref value to the new per device pref value for the
  // current active device. If a previous global setting doesn't exist, we'll
  // use default values of mute = off and volume = 75%.
  void MigrateDeviceMuteSettings(const std::string& active_device);
  void MigrateDeviceVolumeSettings(const std::string& active_device);

  // Notifies the AudioPrefObserver for audio policy pref changes.
  void NotifyAudioPolicyChange();

  scoped_ptr<base::DictionaryValue> device_mute_settings_;
  scoped_ptr<base::DictionaryValue> device_volume_settings_;
  scoped_ptr<base::DictionaryValue> device_state_settings_;

  PrefService* local_state_;  // not owned

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<AudioPrefObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
