// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine.h"

namespace input_method {

InputMethodEngine::InputMethodEngine() {}

InputMethodEngine::~InputMethodEngine() {}

bool InputMethodEngine::SendKeyEvents(
    int context_id,
    const std::vector<KeyboardEvent>& events) {
  // TODO(azurewei) Implement SendKeyEvents funciton
  return false;
}

bool InputMethodEngine::IsActive() const {
  return true;
}

std::string InputMethodEngine::GetExtensionId() const {
  return extension_id_;
}

}  // namespace input_method
