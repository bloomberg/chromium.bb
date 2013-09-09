// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_

#include "chromeos/audio/audio_devices_pref_handler.h"

#include <map>

namespace chromeos {

struct AudioDevice;

// Stub class for AudioDevicesPrefHandler, used for testing.
class CHROMEOS_EXPORT AudioDevicesPrefHandlerStub
    : public AudioDevicesPrefHandler {
 public:
  typedef std::map<uint64, bool> AudioDeviceMute;
  typedef std::map<uint64, int> AudioDeviceVolumeGain;

  AudioDevicesPrefHandlerStub();

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

 protected:
  virtual ~AudioDevicesPrefHandlerStub();

 private:
  AudioDeviceMute audio_device_mute_map_;
  AudioDeviceVolumeGain audio_device_volume_gain_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerStub);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
