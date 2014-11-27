// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/input_method_whitelist.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_methods.h"

namespace chromeos {
namespace input_method {

const char kLanguageDelimiter[] = ",";

InputMethodWhitelist::InputMethodWhitelist() {
  for (size_t i = 0; i < arraysize(kInputMethods); ++i) {
    supported_input_methods_.insert(kInputMethods[i].input_method_id);
  }
}

InputMethodWhitelist::~InputMethodWhitelist() {
}

bool InputMethodWhitelist::InputMethodIdIsWhitelisted(
    const std::string& input_method_id) const {
  return supported_input_methods_.count(input_method_id) > 0;
}

scoped_ptr<InputMethodDescriptors>
InputMethodWhitelist::GetSupportedInputMethods() const {
  scoped_ptr<InputMethodDescriptors> input_methods(new InputMethodDescriptors);
  input_methods->reserve(arraysize(kInputMethods));
  for (size_t i = 0; i < arraysize(kInputMethods); ++i) {
    std::vector<std::string> layouts;
    layouts.push_back(kInputMethods[i].xkb_layout_id);

    std::vector<std::string> languages;
    Tokenize(kInputMethods[i].language_code, kLanguageDelimiter, &languages);
    DCHECK(!languages.empty());

    input_methods->push_back(InputMethodDescriptor(
        extension_ime_util::GetInputMethodIDByEngineID(
            kInputMethods[i].input_method_id),
        "",
        kInputMethods[i].indicator,
        layouts,
        languages,
        kInputMethods[i].is_login_keyboard,
        GURL(), // options page url.
        GURL() // input view page url.
        ));
  }
  return input_methods.Pass();
}

}  // namespace input_method
}  // namespace chromeos
