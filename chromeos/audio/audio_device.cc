// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_device.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace chromeos {

namespace {

// Get the priority for a particular device type. The priority returned
// will be between 0 to 3, the higher number meaning a higher priority.
uint8_t GetDevicePriority(AudioDeviceType type, bool is_input) {
  // Lower the priority of bluetooth mic to prevent unexpected bad eperience
  // to user because of bluetooth audio profile switching. Make priority to
  // zero so this mic will never be automatically chosen.
  if (type == AUDIO_TYPE_BLUETOOTH && is_input)
    return 0;
  switch (type) {
    case AUDIO_TYPE_HEADPHONE:
    case AUDIO_TYPE_MIC:
    case AUDIO_TYPE_USB:
    case AUDIO_TYPE_BLUETOOTH:
      return 3;
    case AUDIO_TYPE_HDMI:
      return 2;
    case AUDIO_TYPE_INTERNAL_SPEAKER:
    case AUDIO_TYPE_INTERNAL_MIC:
      return 1;
    case AUDIO_TYPE_KEYBOARD_MIC:
    case AUDIO_TYPE_HOTWORD:
    case AUDIO_TYPE_POST_MIX_LOOPBACK:
    case AUDIO_TYPE_POST_DSP_LOOPBACK:
    case AUDIO_TYPE_OTHER:
    default:
      return 0;
  }
}

}  // namespace

// static
std::string AudioDevice::GetTypeString(AudioDeviceType type) {
  switch (type) {
    case AUDIO_TYPE_HEADPHONE:
      return "HEADPHONE";
    case AUDIO_TYPE_MIC:
      return "MIC";
    case AUDIO_TYPE_USB:
      return "USB";
    case AUDIO_TYPE_BLUETOOTH:
      return "BLUETOOTH";
    case AUDIO_TYPE_HDMI:
      return "HDMI";
    case AUDIO_TYPE_INTERNAL_SPEAKER:
      return "INTERNAL_SPEAKER";
    case AUDIO_TYPE_INTERNAL_MIC:
      return "INTERNAL_MIC";
    case AUDIO_TYPE_KEYBOARD_MIC:
      return "KEYBOARD_MIC";
    case AUDIO_TYPE_HOTWORD:
      return "HOTWORD";
    case AUDIO_TYPE_POST_MIX_LOOPBACK:
      return "POST_MIX_LOOPBACK";
    case AUDIO_TYPE_POST_DSP_LOOPBACK:
      return "POST_DSP_LOOPBACK";
    case AUDIO_TYPE_OTHER:
    default:
      return "OTHER";
  }
}

// static
AudioDeviceType AudioDevice::GetAudioType(
    const std::string& node_type) {
  if (node_type.find("HEADPHONE") != std::string::npos)
    return AUDIO_TYPE_HEADPHONE;
  else if (node_type.find("INTERNAL_MIC") != std::string::npos)
    return AUDIO_TYPE_INTERNAL_MIC;
  else if (node_type.find("KEYBOARD_MIC") != std::string::npos)
    return AUDIO_TYPE_KEYBOARD_MIC;
  else if (node_type.find("MIC") != std::string::npos)
    return AUDIO_TYPE_MIC;
  else if (node_type.find("USB") != std::string::npos)
    return AUDIO_TYPE_USB;
  else if (node_type.find("BLUETOOTH") != std::string::npos)
    return AUDIO_TYPE_BLUETOOTH;
  else if (node_type.find("HDMI") != std::string::npos)
    return AUDIO_TYPE_HDMI;
  else if (node_type.find("INTERNAL_SPEAKER") != std::string::npos)
    return AUDIO_TYPE_INTERNAL_SPEAKER;
  // TODO(hychao): Remove the 'AOKR' matching line after CRAS switches
  // node type naming to 'HOTWORD'.
  else if (node_type.find("AOKR") != std::string::npos)
    return AUDIO_TYPE_HOTWORD;
  else if (node_type.find("HOTWORD") != std::string::npos)
    return AUDIO_TYPE_HOTWORD;
  else if (node_type.find("POST_MIX_LOOPBACK") != std::string::npos)
    return AUDIO_TYPE_POST_MIX_LOOPBACK;
  else if (node_type.find("POST_DSP_LOOPBACK") != std::string::npos)
    return AUDIO_TYPE_POST_DSP_LOOPBACK;
  else
    return AUDIO_TYPE_OTHER;
}

AudioDevice::AudioDevice()
    : is_input(false),
      id(0),
      stable_device_id(0),
      display_name(""),
      type(AUDIO_TYPE_OTHER),
      priority(0),
      active(false),
      plugged_time(0) {}

AudioDevice::AudioDevice(const AudioNode& node) {
  is_input = node.is_input;
  id = node.id;
  stable_device_id = node.stable_device_id;
  type = GetAudioType(node.type);
  if (!node.name.empty() && node.name != "(default)")
    display_name = node.name;
  else
    display_name = node.device_name;
  device_name = node.device_name;
  mic_positions = node.mic_positions;
  priority = GetDevicePriority(type, node.is_input);
  active = node.active;
  plugged_time = node.plugged_time;
}

AudioDevice::AudioDevice(const AudioDevice& other) = default;

std::string AudioDevice::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = 0x%" PRIx64 " ",
                      id);
  base::StringAppendF(&result,
                      "stable_device_id = 0x%" PRIx64 " ",
                      stable_device_id);
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
  base::StringAppendF(&result, "mic_positions = %s ", mic_positions.c_str());

  return result;
}

}  // namespace chromeos
