// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_device.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

namespace {

std::string GetTypeString(chromeos::AudioDeviceType type) {
  if (type == chromeos::AUDIO_TYPE_INTERNAL)
    return "INTERNAL";
  else if (type == chromeos::AUDIO_TYPE_HEADPHONE)
    return "HEADPHONE";
  else if (type == chromeos::AUDIO_TYPE_USB)
    return "USB";
  else if (type == chromeos::AUDIO_TYPE_BLUETOOTH)
    return "BLUETOOTH";
  else if (type == chromeos::AUDIO_TYPE_HDMI)
    return "HDMI";
  else
    return "OTHER";
}

chromeos::AudioDeviceType GetAudioType(const std::string& node_type) {
  if (node_type.find("INTERNAL_") != std::string::npos)
    return chromeos::AUDIO_TYPE_INTERNAL;
  else if (node_type.find("HEADPHONE") != std::string::npos)
    return chromeos::AUDIO_TYPE_HEADPHONE;
  else if (node_type.find("USB") != std::string::npos)
    return chromeos::AUDIO_TYPE_USB;
  else if (node_type.find("BLUETOOTH") != std::string::npos)
    return chromeos::AUDIO_TYPE_BLUETOOTH;
  else if (node_type.find("HDMI") != std::string::npos)
    return chromeos::AUDIO_TYPE_HDMI;
  else
    return chromeos::AUDIO_TYPE_OTHER;
}

}  // namespace

namespace chromeos {

AudioDevice::AudioDevice()
    : is_input(false),
      id(0),
      active(false) {
}

AudioDevice::AudioDevice(const AudioNode& node) {
  is_input = node.is_input;
  id = node.id;
  type = GetAudioType(node.type);
  if (!node.name.empty() && node.name != "(default)")
    display_name = UTF8ToUTF16(node.name);
  else
    display_name = UTF8ToUTF16(node.device_name);
  active = node.active;
}

std::string AudioDevice::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = %"PRIu64" ",
                      id);
  base::StringAppendF(&result,
                      "display_name = %s ",
                      UTF16ToUTF8(display_name).c_str());
  base::StringAppendF(&result,
                      "type = %s ",
                      GetTypeString(type).c_str());
  base::StringAppendF(&result,
                      "active = %s ",
                      active ? "true" : "false");

  return result;
}

}  // namespace chromeos
