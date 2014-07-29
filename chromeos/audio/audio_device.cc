// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_device.h"

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace {

// Get the priority for a particular device type. The priority returned
// will be between 0 to 3, the higher number meaning a higher priority.
uint8 GetDevicePriority(chromeos::AudioDeviceType type) {
  switch (type) {
    // Fall through.
    case chromeos::AUDIO_TYPE_HEADPHONE:
    case chromeos::AUDIO_TYPE_MIC:
    case chromeos::AUDIO_TYPE_USB:
    case chromeos::AUDIO_TYPE_BLUETOOTH:
      return 3;
    case chromeos::AUDIO_TYPE_HDMI:
      return 2;
      // Fall through.
    case chromeos::AUDIO_TYPE_INTERNAL_SPEAKER:
    case chromeos::AUDIO_TYPE_INTERNAL_MIC:
      return 1;
      // Fall through.
    case chromeos::AUDIO_TYPE_KEYBOARD_MIC:
    case chromeos::AUDIO_TYPE_OTHER:
    default:
      return 0;
  }
}

std::string GetTypeString(chromeos::AudioDeviceType type) {
  switch (type) {
    case chromeos::AUDIO_TYPE_HEADPHONE:
      return "HEADPHONE";
    case chromeos::AUDIO_TYPE_MIC:
      return "MIC";
    case chromeos::AUDIO_TYPE_USB:
      return "USB";
    case chromeos::AUDIO_TYPE_BLUETOOTH:
      return "BLUETOOTH";
    case chromeos::AUDIO_TYPE_HDMI:
      return "HDMI";
    case chromeos::AUDIO_TYPE_INTERNAL_SPEAKER:
      return "INTERNAL_SPEAKER";
    case chromeos::AUDIO_TYPE_INTERNAL_MIC:
      return "INTERNAL_MIC";
    case chromeos::AUDIO_TYPE_KEYBOARD_MIC:
      return "KEYBOARD_MIC";
    case chromeos::AUDIO_TYPE_OTHER:
    default:
      return "OTHER";
  }
}

chromeos::AudioDeviceType GetAudioType(const std::string& node_type) {
  if (node_type.find("HEADPHONE") != std::string::npos)
    return chromeos::AUDIO_TYPE_HEADPHONE;
  else if (node_type.find("INTERNAL_MIC") != std::string::npos)
    return chromeos::AUDIO_TYPE_INTERNAL_MIC;
  else if (node_type.find("KEYBOARD_MIC") != std::string::npos)
    return chromeos::AUDIO_TYPE_KEYBOARD_MIC;
  else if (node_type.find("MIC") != std::string::npos)
    return chromeos::AUDIO_TYPE_MIC;
  else if (node_type.find("USB") != std::string::npos)
    return chromeos::AUDIO_TYPE_USB;
  else if (node_type.find("BLUETOOTH") != std::string::npos)
    return chromeos::AUDIO_TYPE_BLUETOOTH;
  else if (node_type.find("HDMI") != std::string::npos)
    return chromeos::AUDIO_TYPE_HDMI;
  else if (node_type.find("INTERNAL_SPEAKER") != std::string::npos)
    return chromeos::AUDIO_TYPE_INTERNAL_SPEAKER;
  else
    return chromeos::AUDIO_TYPE_OTHER;
}

}  // namespace

namespace chromeos {

AudioDevice::AudioDevice()
    : is_input(false),
      id(0),
      display_name(""),
      type(AUDIO_TYPE_OTHER),
      priority(0),
      active(false),
      plugged_time(0) {
}

AudioDevice::AudioDevice(const AudioNode& node) {
  is_input = node.is_input;
  id = node.id;
  type = GetAudioType(node.type);
  if (!node.name.empty() && node.name != "(default)")
    display_name = node.name;
  else
    display_name = node.device_name;
  device_name = node.device_name;
  priority = GetDevicePriority(type);
  active = node.active;
  plugged_time = node.plugged_time;
}

std::string AudioDevice::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = 0x%" PRIx64 " ",
                      id);
  base::StringAppendF(&result,
                      "display_name = %s ",
                      display_name.c_str());
  base::StringAppendF(&result,
                      "device_name = %s ",
                      device_name.c_str());
  base::StringAppendF(&result,
                      "type = %s ",
                      GetTypeString(type).c_str());
  base::StringAppendF(&result,
                      "active = %s ",
                      active ? "true" : "false");
  base::StringAppendF(&result,
                      "plugged_time= %s ",
                      base::Uint64ToString(plugged_time).c_str());

  return result;
}

}  // namespace chromeos
