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
    const std::string& keyboard_layout,
    const std::string& language_code,
    const std::string& options_page_url)
    : id_(id),
      name_(name),
      keyboard_layout_(keyboard_layout),
      language_code_(language_code),
      options_page_url_(options_page_url) {
}

InputMethodDescriptor::InputMethodDescriptor() {
}

InputMethodDescriptor::~InputMethodDescriptor() {
}

bool InputMethodDescriptor::operator==(
    const InputMethodDescriptor& other) const {
  return id() == other.id();
}

bool InputMethodDescriptor::operator!=(
    const InputMethodDescriptor& other) const {
  return !(*this == other);
}

}  // namespace input_method
}  // namespace chromeos
