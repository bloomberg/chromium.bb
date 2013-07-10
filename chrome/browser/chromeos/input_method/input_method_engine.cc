// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#include "chrome/browser/chromeos/input_method/input_method_engine_ibus.h"

namespace chromeos {

InputMethodEngine::KeyboardEvent::KeyboardEvent()
    : alt_key(false),
      ctrl_key(false),
      shift_key(false) {
}

InputMethodEngine::KeyboardEvent::~KeyboardEvent() {
}

InputMethodEngine::MenuItem::MenuItem() {
}

InputMethodEngine::MenuItem::~MenuItem() {
}

InputMethodEngine::Candidate::Candidate() {
}

InputMethodEngine::Candidate::~Candidate() {
}

InputMethodEngine::Observer::~Observer() {
}

InputMethodEngine* InputMethodEngine::CreateEngine(
    InputMethodEngine::Observer* observer,
    const char* engine_name,
    const char* extension_id,
    const char* engine_id,
    const char* description,
    const std::vector<std::string>& languages,
    const std::vector<std::string>& layouts,
    const GURL& options_page,
    std::string* error) {

  InputMethodEngineIBus* engine = new InputMethodEngineIBus();
  engine->Initialize(observer,
                     engine_name,
                     extension_id,
                     engine_id,
                     description,
                     languages,
                     layouts,
                     options_page,
                     error);
  return engine;
}

}  // namespace chromeos
