// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/input_method_descriptor.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace chromeos {
namespace input_method {

InputMethodDescriptor::InputMethodDescriptor(
    const std::string& id,
    const std::string& name,
    const std::vector<std::string>& keyboard_layouts,
    const std::string& language_code,
    const std::string& options_page_url)
    : id_(id),
      name_(name),
      keyboard_layouts_(keyboard_layouts),
      language_code_(language_code),
      options_page_url_(options_page_url) {
}

std::string InputMethodDescriptor::GetPreferredKeyboardLayout() const {
  // TODO(nona): Investigate better way to guess the preferred layout
  //             http://crbug.com/170601.
  return keyboard_layouts_.empty() ? "us" : keyboard_layouts_[0];
}

InputMethodDescriptor::InputMethodDescriptor() {
}

InputMethodDescriptor::~InputMethodDescriptor() {
}

}  // namespace input_method
}  // namespace chromeos
