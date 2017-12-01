// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_name.h"
#include "media/audio/audio_device_description.h"

namespace media {

AudioDeviceName::AudioDeviceName() = default;

AudioDeviceName::AudioDeviceName(const std::string& device_name,
                                 const std::string& unique_id)
    : device_name(device_name),
      unique_id(unique_id) {
}

// static
AudioDeviceName AudioDeviceName::CreateDefault() {
  return AudioDeviceName(AudioDeviceDescription::GetDefaultDeviceName(),
                         AudioDeviceDescription::kDefaultDeviceId);
}

// static
AudioDeviceName AudioDeviceName::CreateCommunications() {
  return AudioDeviceName(AudioDeviceDescription::GetCommunicationsDeviceName(),
                         AudioDeviceDescription::kCommunicationsDeviceId);
}

}  // namespace media
