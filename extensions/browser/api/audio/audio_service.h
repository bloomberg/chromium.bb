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

using OutputInfo = std::vector<linked_ptr<core_api::audio::OutputDeviceInfo>>;
using InputInfo = std::vector<linked_ptr<core_api::audio::InputDeviceInfo>>;
using DeviceIdList = std::vector<std::string>;

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

  // Sets the active nodes to the nodes specified by |device_list|.
  // It can pass in the "complete" active node list of either input
  // nodes, or output nodes, or both. If only input nodes are passed in,
  // it will only change the input nodes' active status, output nodes will NOT
  // be changed; similarly for the case if only output nodes are passed.
  // If the nodes specified in |new_active_ids| are already active, they will
  // remain active. Otherwise, the old active nodes will be de-activated before
  // we activate the new nodes with the same type(input/output).
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
