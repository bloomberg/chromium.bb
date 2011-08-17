// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_KEY_CONVERTER_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_KEY_CONVERTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/test/webdriver/webdriver_automation.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace webdriver {

// Convenience functions for creating |WebKeyEvent|s. Used by unittests.
WebKeyEvent CreateKeyDownEvent(ui::KeyboardCode key_code, int modifiers);
WebKeyEvent CreateKeyUpEvent(ui::KeyboardCode key_code, int modifiers);
WebKeyEvent CreateCharEvent(const std::string& unmodified_text,
                            const std::string& modified_text,
                            int modifiers);

// Converts keys into appropriate |WebKeyEvent|s. This will do a best effort
// conversion. However, if the input is invalid it will return false and set
// an error message.
bool ConvertKeysToWebKeyEvents(const string16& keys,
                               std::vector<WebKeyEvent>* key_events,
                               std::string* error_msg);

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_KEY_CONVERTER_H_
