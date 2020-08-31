// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Class which implements AudioDevicesPrefHandler interface and register audio
// preferences as well.
class COMPONENT_EXPORT(CHROMEOS_AUDIO) AudioDevicesPrefHandlerImpl
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

  void SetDeviceActive(const AudioDevice& device,
                       bool active,
                       bool activate_by_user) override;
  bool GetDeviceActive(const AudioDevice& device,
                       bool* active,
                       bool* activate_by_user) override;

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

  // Migrates devices state pref for an audio device. Device settings are
  // saved under device stable device ID - this method migrates device state
  // for a device that is saved under key derived from v1 stable device ID to
  // |device_key|. Note that |device_key| should be the key derived from
  // |device|'s v2 stable device ID.
  bool MigrateDevicesStatePref(const std::string& device_key,
                               const AudioDevice& device);

  // Methods to migrate the mute and volume settings for an audio device.
  // Migration is done in the folowing way:
  //   1. If there is a setting for the device under |device_key|, do nothing.
  //      (Note that |device_key| is expected to be the key derived from
  //       |device's| v2 stable device ID).
  //   2. If there is a setting for the device under the key derived from
  //      |device|'s v1 stable device ID, move the value to |device_key|.
  //   3. If a previous global pref value exists, move it to the per device
  //      setting (under |device_key|).
  //   4. If a previous global setting is not set, use default values of
  //      mute = off and volume = 75%.
  void MigrateDeviceMuteSettings(const std::string& device_key,
                                 const AudioDevice& device);
  void MigrateDeviceVolumeGainSettings(const std::string& device_key,
                                       const AudioDevice& device);

  // Notifies the AudioPrefObserver for audio policy pref changes.
  void NotifyAudioPolicyChange();

  std::unique_ptr<base::DictionaryValue> device_mute_settings_;
  std::unique_ptr<base::DictionaryValue> device_volume_settings_;
  std::unique_ptr<base::DictionaryValue> device_state_settings_;

  PrefService* local_state_;  // not owned

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<AudioPrefObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
