// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_DEVICE_H_
#define CHROMEOS_AUDIO_AUDIO_DEVICE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

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
  AUDIO_TYPE_AOKR,
  AUDIO_TYPE_POST_MIX_LOOPBACK,
  AUDIO_TYPE_POST_DSP_LOOPBACK,
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

  // Indicates that an input or output audio device is for simple usage like
  // playback or recording for user. In contrast, audio device such as
  // loopback, always on keyword recognition (AOKR), and keyboard mic are
  // not for simple usage.
  bool is_for_simple_usage() const {
    return (type == AUDIO_TYPE_HEADPHONE ||
            type == AUDIO_TYPE_INTERNAL_MIC ||
            type == AUDIO_TYPE_MIC ||
            type == AUDIO_TYPE_USB ||
            type == AUDIO_TYPE_BLUETOOTH ||
            type == AUDIO_TYPE_HDMI ||
            type == AUDIO_TYPE_INTERNAL_SPEAKER);
  }

  bool is_input;
  uint64_t id;
  std::string display_name;
  std::string device_name;
  std::string mic_positions;
  AudioDeviceType type;
  uint8_t priority;
  bool active;
  uint64_t plugged_time;
};

typedef std::vector<AudioDevice> AudioDeviceList;
typedef std::map<uint64_t, AudioDevice> AudioDeviceMap;

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
