// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICE_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/audio_node.h"

namespace chromeos {

enum AudioDeviceType {
  AUDIO_TYPE_INTERNAL,
  AUDIO_TYPE_HEADPHONE,
  AUDIO_TYPE_USB,
  AUDIO_TYPE_BLUETOOTH,
  AUDIO_TYPE_HDMI,
  AUDIO_TYPE_OTHER,
};

struct CHROMEOS_EXPORT AudioDevice {
  bool is_input;
  uint64 id;
  base::string16 display_name;
  AudioDeviceType type;
  bool active;

  AudioDevice();
  explicit AudioDevice(const AudioNode& node);
  std::string ToString() const;
};

typedef std::vector<AudioDevice> AudioDeviceList;

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICE_H_
