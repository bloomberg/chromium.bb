// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/fake_input_method_library.h"

namespace chromeos {

FakeInputMethodLibrary::FakeInputMethodLibrary()
    : previous_input_method_("", "", "", ""),
      current_input_method_("", "", "", "") {
}

void FakeInputMethodLibrary::AddObserver(Observer* observer) {
}

void FakeInputMethodLibrary::RemoveObserver(Observer* observer) {
}

chromeos::InputMethodDescriptors*
FakeInputMethodLibrary::GetActiveInputMethods() {
  return CreateFallbackInputMethodDescriptors();
}

size_t FakeInputMethodLibrary::GetNumActiveInputMethods() {
  scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
  return input_methods->size();
}

chromeos::InputMethodDescriptors*
FakeInputMethodLibrary::GetSupportedInputMethods() {
  return CreateFallbackInputMethodDescriptors();
}

void FakeInputMethodLibrary::ChangeInputMethod(
    const std::string& input_method_id) {
}

void FakeInputMethodLibrary::SetImePropertyActivated(const std::string& key,
                                                     bool activated) {
}

bool FakeInputMethodLibrary::InputMethodIsActivated(
    const std::string& input_method_id) {
  return true;
}

bool FakeInputMethodLibrary::GetImeConfig(
    const char* section, const char* config_name, ImeConfigValue* out_value) {
  return true;
}

bool FakeInputMethodLibrary::SetImeConfig(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  return true;
}

}  // namespace chromeos
