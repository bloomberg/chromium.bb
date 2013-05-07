// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/common/extensions/api/audio.h"

namespace extensions {

typedef std::vector<linked_ptr<api::audio::OutputDeviceInfo> > OutputInfo;
typedef std::vector<linked_ptr<api::audio::InputDeviceInfo> > InputInfo;
typedef std::vector<std::string> DeviceIdList;

class AudioService {
 public:
  class Observer {
   public:
    // Called when anything changes to the audio device configuration.
    virtual void OnDeviceChanged() = 0;

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

  // Set the devices in the following list as active. This will only pick
  // the first input and first active devices to set to active.
  virtual void SetActiveDevices(const DeviceIdList& device_list) = 0;

  // Set the muted and volume/gain properties of a device.
  virtual bool SetDeviceProperties(const std::string& device_id,
                                   bool muted,
                                   int volume,
                                   int gain) = 0;

 protected:
  AudioService() {}

  DISALLOW_COPY_AND_ASSIGN(AudioService);
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_SERVICE_H_
