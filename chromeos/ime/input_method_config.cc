// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/input_method_config.h"

#include <sstream>

#include "base/logging.h"

namespace chromeos {
namespace input_method {

InputMethodConfigValue::InputMethodConfigValue()
    : type(kValueTypeString),
      int_value(0),
      bool_value(false) {
}

InputMethodConfigValue::~InputMethodConfigValue() {
}

std::string InputMethodConfigValue::ToString() const {
  std::stringstream stream;
  stream << "type=" << type;
  switch (type) {
    case kValueTypeString:
      stream << ", string_value=" << string_value;
      break;
    case kValueTypeInt:
      stream << ", int_value=" << int_value;
      break;
    case kValueTypeBool:
      stream << ", bool_value=" << (bool_value ? "true" : "false");
      break;
    case kValueTypeStringList:
      stream << ", string_list_value=";
      for (size_t i = 0; i < string_list_value.size(); ++i) {
        if (i)
          stream << ",";
        stream << string_list_value[i];
      }
      break;
  }
  return stream.str();
}

}  // namespace input_method
}  // namespace chromeos
