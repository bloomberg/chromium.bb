// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICE_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/audio_node.h"

namespace chromeos {

// Ordered from the highest priority to the lowest.
enum AudioDeviceType {
  AUDIO_TYPE_HEADPHONE,
  AUDIO_TYPE_MIC,
  AUDIO_TYPE_USB,
  AUDIO_TYPE_BLUETOOTH,
  AUDIO_TYPE_HDMI,
  AUDIO_TYPE_INTERNAL_SPEAKER,
  AUDIO_TYPE_INTERNAL_MIC,
  AUDIO_TYPE_KEYBOARD_MIC,
  AUDIO_TYPE_OTHER,
};

struct CHROMEOS_EXPORT AudioDevice {
  AudioDevice();
  explicit AudioDevice(const AudioNode& node);
  std::string ToString() const;

  // Converts between the string type sent via D-Bus and AudioDeviceType.
  // Static so they can be used by tests.
  static std::string GetTypeString(chromeos::AudioDeviceType type);
  static chromeos::AudioDeviceType GetAudioType(const std::string& node_type);

  bool is_input;
  uint64 id;
  std::string display_name;
  std::string device_name;
  AudioDeviceType type;
  uint8 priority;
  bool active;
  uint64 plugged_time;
};

typedef std::vector<AudioDevice> AudioDeviceList;
typedef std::map<uint64, AudioDevice> AudioDeviceMap;

struct AudioDeviceCompare {
  // Rules used to discern which device is higher,
  // 1.) Device Type:
  //       [Headphones/USB/Bluetooh > HDMI > Internal Speakers]
  //       [External Mic/USB Mic/Bluetooth > Internal Mic]
  // 2.) Device Plugged in Time:
  //       [Later > Earlier]
  bool operator()(const chromeos::AudioDevice& a,
                  const chromeos::AudioDevice& b) const {
    if (a.priority < b.priority) {
      return true;
    } else if (b.priority < a.priority) {
      return false;
    } else if (a.plugged_time < b.plugged_time) {
      return true;
    } else {
      return false;
    }
  }
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_DEVICE_H_
