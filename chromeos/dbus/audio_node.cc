// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/audio_node.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

AudioNode::AudioNode()
    : is_input(false),
      id(0),
      active(false),
      plugged_time(0) {
}

AudioNode::AudioNode(bool is_input,
                     uint64_t id,
                     std::string device_name,
                     std::string type,
                     std::string name,
                     bool active,
                     uint64_t plugged_time)
    : is_input(is_input),
      id(id),
      device_name(device_name),
      type(type),
      name(name),
      active(active),
      plugged_time(plugged_time) {}

AudioNode::~AudioNode() {}

std::string AudioNode::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = 0x%" PRIx64 " ",
                      id);
  base::StringAppendF(&result,
                      "device_name = %s ",
                      device_name.c_str());
  base::StringAppendF(&result,
                      "type = %s ",
                      type.c_str());
  base::StringAppendF(&result,
                      "name = %s ",
                      name.c_str());
  base::StringAppendF(&result,
                      "active = %s ",
                      active ? "true" : "false");
  base::StringAppendF(&result,
                      "plugged_time= %s ",
                      base::Uint64ToString(plugged_time).c_str());

  return result;
}

}  // namespace chromeos
