// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio/audio_service.h"

namespace extensions {

class AudioServiceImpl : public AudioService {
 public:
  AudioServiceImpl() {}
  virtual ~AudioServiceImpl() {}

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // Start to query audio device information.
  virtual void StartGetInfo(const GetInfoCallback& callback) OVERRIDE;
  virtual void SetActiveDevices(const DeviceIdList& device_list) OVERRIDE;
  virtual bool SetDeviceProperties(const std::string& device_id,
                                   bool muted,
                                   int volume,
                                   int gain) OVERRIDE;
};

void AudioServiceImpl::AddObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

void AudioServiceImpl::RemoveObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

AudioService* AudioService::CreateInstance() {
  return new AudioServiceImpl;
}

void AudioServiceImpl::StartGetInfo(const GetInfoCallback& callback) {
  // TODO: implement this for platforms other than Chrome OS.
  if (!callback.is_null())
    callback.Run(OutputInfo(), InputInfo(), false);
}

void AudioServiceImpl::SetActiveDevices(const DeviceIdList& device_list) {
}

bool AudioServiceImpl::SetDeviceProperties(const std::string& device_id,
                                           bool muted,
                                           int volume,
                                           int gain) {
  return false;
}

}  // namespace extensions
