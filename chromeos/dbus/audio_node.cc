// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/audio_node.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"

namespace chromeos {

AudioNode::AudioNode()
    : is_input(false),
      id(0),
      active(false),
      plugged_time(0) {
}

std::string AudioNode::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "is_input = %s ",
                      is_input ? "true" : "false");
  base::StringAppendF(&result,
                      "id = %s ",
                      base::Uint64ToString(id).c_str());
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
