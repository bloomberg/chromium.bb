// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_KEYCODE_TEXT_CONVERSION_H_
#define CHROME_TEST_WEBDRIVER_KEYCODE_TEXT_CONVERSION_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace webdriver {

// These functions only support conversion of characters in the BMP
// (Basic Multilingual Plane).

// Returns the text in UTF8 that would be produced from the given key code and
// modifiers with the current keyboard layout, or "" if no character would
// be produced. This will return "" if the produced text contains characters
// outside the BMP.
std::string ConvertKeyCodeToText(ui::KeyboardCode key_code, int modifiers);

// Converts a character to the key code and modifiers that would produce
// the character using the current keyboard layout. Returns true on success.
bool ConvertCharToKeyCode(
    char16 key, ui::KeyboardCode* key_code, int *necessary_modifiers);

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_KEYCODE_TEXT_CONVERSION_H_
