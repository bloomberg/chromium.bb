// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace chromeos {
namespace input_method {

namespace {
const char kFallbackLayout[] = "us";
}  // namespace

InputMethodDescriptor::InputMethodDescriptor(const std::string& id,
                                             const std::string& name,
                                             const std::string& keyboard_layout,
                                             const std::string& language_code,
                                             bool third_party)
    : id_(id),
      name_(name),
      keyboard_layout_(keyboard_layout),
      language_code_(language_code),
      third_party_(third_party) {
}

InputMethodDescriptor::InputMethodDescriptor() : third_party_(false) {
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

// static
InputMethodDescriptor
InputMethodDescriptor::GetFallbackInputMethodDescriptor() {
  return InputMethodDescriptor("xkb:us::eng",
                               "",
                               kFallbackLayout,
                               "en-US",
                               false);
}

std::string InputMethodDescriptor::ToString() const {
  std::stringstream stream;
  stream << "id=" << id()
         << ", name=" << name()
         << ", keyboard_layout=" << keyboard_layout()
         << ", language_code=" << language_code()
         << ", third_party=" << third_party();
  return stream.str();
}

}  // namespace input_method
}  // namespace chromeos
