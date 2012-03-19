// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"

#include <vector>

#include "chrome/browser/chromeos/input_method/ibus_input_methods.h"
#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"

namespace chromeos {
namespace input_method {

InputMethodWhitelist::InputMethodWhitelist() {
  for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
    supported_input_methods_.insert(kIBusEngines[i].input_method_id);
  }
  for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
    supported_layouts_.insert(kIBusEngines[i].xkb_layout_id);
  }
}

InputMethodWhitelist::~InputMethodWhitelist() {
}

bool InputMethodWhitelist::InputMethodIdIsWhitelisted(
    const std::string& input_method_id) const {
  return supported_input_methods_.count(input_method_id) > 0;
}

bool InputMethodWhitelist::XkbLayoutIsSupported(
    const std::string& xkb_layout) const {
  return supported_layouts_.count(xkb_layout) > 0;
}

InputMethodDescriptors* InputMethodWhitelist::GetSupportedInputMethods() const {
  InputMethodDescriptors* input_methods = new InputMethodDescriptors;
  input_methods->reserve(arraysize(kIBusEngines));
  for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
    input_methods->push_back(InputMethodDescriptor(
        *this,
        kIBusEngines[i].input_method_id,
        "",
        kIBusEngines[i].xkb_layout_id,
        kIBusEngines[i].language_code));
  }
  return input_methods;
}

}  // namespace input_method
}  // namespace chromeos
