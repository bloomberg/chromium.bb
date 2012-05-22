// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"

#include <sstream>

#include "base/logging.h"
#include "base/string_split.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"

// TODO(yusukes): Remove virtual keyboard support.

namespace chromeos {
namespace input_method {

namespace {
const char kFallbackLayout[] = "us";
}  // namespace

InputMethodDescriptor::InputMethodDescriptor(
    const InputMethodWhitelist& whitelist,
    const std::string& id,
    const std::string& name,
    const std::string& raw_layout,
    const std::string& language_code)
    : id_(id),
      name_(name),
      language_code_(language_code) {
  keyboard_layout_ = kFallbackLayout;
  base::SplitString(raw_layout, ',', &virtual_keyboard_layouts_);

  // Find a valid XKB layout name from the comma-separated list, |raw_layout|.
  // Only the first acceptable XKB layout name in the list is used as the
  // |keyboard_layout| value of the InputMethodDescriptor object to be created
  for (size_t i = 0; i < virtual_keyboard_layouts_.size(); ++i) {
    if (whitelist.XkbLayoutIsSupported(virtual_keyboard_layouts_[i])) {
      keyboard_layout_ = virtual_keyboard_layouts_[i];
      DCHECK(keyboard_layout_.find(",") == std::string::npos);
      break;
    }
  }
}

InputMethodDescriptor::InputMethodDescriptor() {
}

InputMethodDescriptor::~InputMethodDescriptor() {
}

InputMethodDescriptor::InputMethodDescriptor(
    const std::string& in_id,
    const std::string& in_name,
    const std::string& in_keyboard_layout,
    const std::string& in_virtual_keyboard_layouts,
    const std::string& in_language_code)
    : id_(in_id),
      name_(in_name),
      keyboard_layout_(in_keyboard_layout),
      language_code_(in_language_code) {
  DCHECK(keyboard_layout_.find(",") == std::string::npos);
  base::SplitString(
      in_virtual_keyboard_layouts, ',', &virtual_keyboard_layouts_);
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
                               kFallbackLayout,
                               "en-US");
}

std::string InputMethodDescriptor::ToString() const {
  std::stringstream stream;
  stream << "id=" << id()
         << ", name=" << name()
         << ", keyboard_layout=" << keyboard_layout()
         << ", virtual_keyboard_layouts=" << virtual_keyboard_layouts_.size()
         << ", language_code=" << language_code();
  return stream.str();
}

}  // namespace input_method
}  // namespace chromeos
