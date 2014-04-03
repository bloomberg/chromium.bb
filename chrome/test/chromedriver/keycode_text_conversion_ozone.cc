// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"

// TODO(arunprasadr) Implement these functions properly for ozone platforms.
bool ConvertKeyCodeToText(
    ui::KeyboardCode key_code, int modifiers, std::string* text,
        std::string* error_msg) {
  *text = std::string();
  *error_msg = std::string("Not Implemented");
  NOTIMPLEMENTED();
  return false;
}

bool ConvertCharToKeyCode(
    base::char16 key, ui::KeyboardCode* key_code, int *necessary_modifiers,
        std::string* error_msg) {
  *error_msg = std::string("Not Implemented");
  *necessary_modifiers = 0;
  NOTIMPLEMENTED();
  return false;
}
