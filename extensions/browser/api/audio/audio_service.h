// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "extensions/common/api/audio.h"

namespace extensions {

using OutputInfo = std::vector<linked_ptr<api::audio::OutputDeviceInfo>>;
using InputInfo = std::vector<linked_ptr<api::audio::InputDeviceInfo>>;
using DeviceIdList = std::vector<std::string>;
using DeviceInfoList = std::vector<linked_ptr<api::audio::AudioDeviceInfo>>;

class AudioService {
 public:
  class Observer {
   public:
    // Called when anything changes to the audio device configuration.
    virtual void OnDeviceChanged() = 0;

    // Called when the sound level of an active audio device changes.
    virtual void OnLevelChanged(const std::string& id, int level) = 0;

    // Called when the mute state of audio input/output changes.
    virtual void OnMuteChanged(bool is_input, bool is_muted) = 0;

    // Called when the audio devices change, either new devices being added, or
    // existing devices being removed.
    virtual void OnDevicesChanged(const DeviceInfoList&) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Callback type for completing to get audio device information.
  typedef base::Callback<void(const OutputInfo&, const InputInfo&, bool)>
      GetInfoCallback;

  // Creates a platform-specific AudioService instance.
  static AudioService* CreateInstance();

  virtual ~AudioService() {}

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Start to query audio device information. Should be called on UI thread.
  // The |callback| will be invoked once the query is completed.
  virtual void StartGetInfo(const GetInfoCallback& callback) = 0;

  // Sets the active devices to the devices specified by |device_list|.
  // It can pass in the "complete" active device list of either input
  // devices, or output devices, or both. If only input devices are passed in,
  // it will only change the input devices' active status, output devices will
  // NOT be changed; similarly for the case if only output devices are passed.
  // If the devices specified in |new_active_ids| are already active, they will
  // remain active. Otherwise, the old active devices will be de-activated
  // before we activate the new devices with the same type(input/output).
  virtual void SetActiveDevices(const DeviceIdList& device_list) = 0;

  // Set the muted and volume/gain properties of a device.
  virtual bool SetDeviceProperties(const std::string& device_id,
                                   bool muted,
                                   int volume,
                                   int gain) = 0;

 protected:
  AudioService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_
