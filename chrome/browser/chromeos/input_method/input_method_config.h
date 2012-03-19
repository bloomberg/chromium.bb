// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIG_H_
#pragma once

#include <string>
#include <vector>

namespace chromeos {
namespace input_method {

// A structure which represents a value of an input method configuration item.
// This struct is used by SetInputMethodConfig().
struct InputMethodConfigValue {
  InputMethodConfigValue();
  ~InputMethodConfigValue();

  // Debug print function.
  std::string ToString() const;

  enum ValueType {
    kValueTypeString = 0,
    kValueTypeInt,
    kValueTypeBool,
    kValueTypeStringList,
  };

  // A value is stored on |string_value| member if |type| is kValueTypeString.
  // The same is true for other enum values.
  ValueType type;

  std::string string_value;
  int int_value;
  bool bool_value;
  std::vector<std::string> string_list_value;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONFIG_H_
