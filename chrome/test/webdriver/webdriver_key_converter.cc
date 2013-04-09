// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_key_converter.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/webdriver/keycode_text_conversion.h"
#include "chrome/test/webdriver/webdriver_logging.h"

namespace {

struct ModifierMaskAndKeyCode {
  int mask;
  ui::KeyboardCode key_code;
};

const ModifierMaskAndKeyCode kModifiers[] = {
  { automation::kShiftKeyMask, ui::VKEY_SHIFT },
  { automation::kControlKeyMask, ui::VKEY_CONTROL },
  { automation::kAltKeyMask, ui::VKEY_MENU }
};

// TODO(kkania): Use this in KeyMap.
// Ordered list of all the key codes corresponding to special WebDriver keys.
// These WebDriver keys are defined in the Unicode Private Use Area.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/value
const ui::KeyboardCode kSpecialWebDriverKeys[] = {
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_HELP,
    ui::VKEY_BACK,
    ui::VKEY_TAB,
    ui::VKEY_CLEAR,
    ui::VKEY_RETURN,
    ui::VKEY_RETURN,
    ui::VKEY_SHIFT,
    ui::VKEY_CONTROL,
    ui::VKEY_MENU,
    ui::VKEY_PAUSE,
    ui::VKEY_ESCAPE,
    ui::VKEY_SPACE,
    ui::VKEY_PRIOR,    // page up
    ui::VKEY_NEXT,     // page down
    ui::VKEY_END,
    ui::VKEY_HOME,
    ui::VKEY_LEFT,
    ui::VKEY_UP,
    ui::VKEY_RIGHT,
    ui::VKEY_DOWN,
    ui::VKEY_INSERT,
    ui::VKEY_DELETE,
    ui::VKEY_OEM_1,     // semicolon
    ui::VKEY_OEM_PLUS,  // equals
    ui::VKEY_NUMPAD0,
    ui::VKEY_NUMPAD1,
    ui::VKEY_NUMPAD2,
    ui::VKEY_NUMPAD3,
    ui::VKEY_NUMPAD4,
    ui::VKEY_NUMPAD5,
    ui::VKEY_NUMPAD6,
    ui::VKEY_NUMPAD7,
    ui::VKEY_NUMPAD8,
    ui::VKEY_NUMPAD9,
    ui::VKEY_MULTIPLY,
    ui::VKEY_ADD,
    ui::VKEY_OEM_COMMA,
    ui::VKEY_SUBTRACT,
    ui::VKEY_DECIMAL,
    ui::VKEY_DIVIDE,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_UNKNOWN,
    ui::VKEY_F1,
    ui::VKEY_F2,
    ui::VKEY_F3,
    ui::VKEY_F4,
    ui::VKEY_F5,
    ui::VKEY_F6,
    ui::VKEY_F7,
    ui::VKEY_F8,
    ui::VKEY_F9,
    ui::VKEY_F10,
    ui::VKEY_F11,
    ui::VKEY_F12};

const char16 kWebDriverNullKey = 0xE000U;
const char16 kWebDriverShiftKey = 0xE008U;
const char16 kWebDriverControlKey = 0xE009U;
const char16 kWebDriverAltKey = 0xE00AU;
const char16 kWebDriverCommandKey = 0xE03DU;

// Returns whether the given key is a WebDriver key modifier.
bool IsModifierKey(char16 key) {
  switch (key) {
    case kWebDriverShiftKey:
    case kWebDriverControlKey:
    case kWebDriverAltKey:
    case kWebDriverCommandKey:
      return true;
    default:
      return false;
  }
}

// Gets the key code associated with |key|, if it is a special WebDriver key.
// Returns whether |key| is a special WebDriver key. If true, |key_code| will
// be set.
bool KeyCodeFromSpecialWebDriverKey(char16 key, ui::KeyboardCode* key_code) {
  int index = static_cast<int>(key) - 0xE000U;
  bool is_special_key = index >= 0 &&
      index < static_cast<int>(arraysize(kSpecialWebDriverKeys));
  if (is_special_key)
    *key_code = kSpecialWebDriverKeys[index];
  return is_special_key;
}

// Gets the key code associated with |key|, if it is a special shorthand key.
// Shorthand keys are common text equivalents for keys, such as the newline
// character, which is shorthand for the return key. Returns whether |key| is
// a shorthand key. If true, |key_code| will be set and |client_should_skip|
// will be set to whether the key should be skipped.
bool KeyCodeFromShorthandKey(char16 key_utf16,
                             ui::KeyboardCode* key_code,
                             bool* client_should_skip) {
  string16 key_str_utf16;
  key_str_utf16.push_back(key_utf16);
  std::string key_str_utf8 = UTF16ToUTF8(key_str_utf16);
  if (key_str_utf8.length() != 1)
    return false;
  bool should_skip = false;
  char key = key_str_utf8[0];
  if (key == '\n') {
    *key_code = ui::VKEY_RETURN;
  } else if (key == '\t') {
    *key_code = ui::VKEY_TAB;
  } else if (key == '\b') {
    *key_code = ui::VKEY_BACK;
  } else if (key == ' ') {
    *key_code = ui::VKEY_SPACE;
  } else if (key == '\r') {
    *key_code = ui::VKEY_UNKNOWN;
    should_skip = true;
  } else {
    return false;
  }
  *client_should_skip = should_skip;
  return true;
}

}  // namespace

namespace webdriver {

WebKeyEvent CreateKeyDownEvent(ui::KeyboardCode key_code, int modifiers) {
  return WebKeyEvent(automation::kRawKeyDownType,
                     key_code,
                     std::string(),
                     std::string(),
                     modifiers);
}

WebKeyEvent CreateKeyUpEvent(ui::KeyboardCode key_code, int modifiers) {
  return WebKeyEvent(automation::kKeyUpType,
                     key_code,
                     std::string(),
                     std::string(),
                     modifiers);
}

WebKeyEvent CreateCharEvent(const std::string& unmodified_text,
                            const std::string& modified_text,
                            int modifiers) {
  return WebKeyEvent(automation::kCharType,
                     ui::VKEY_UNKNOWN,
                     unmodified_text,
                     modified_text,
                     modifiers);
}

bool ConvertKeysToWebKeyEvents(const string16& client_keys,
                               const Logger& logger,
                               bool release_modifiers,
                               int* modifiers,
                               std::vector<WebKeyEvent>* client_key_events,
                               std::string* error_msg) {
  std::vector<WebKeyEvent> key_events;

  string16 keys = client_keys;
  // Add an implicit NULL character to the end of the input to depress all
  // modifiers.
  if (release_modifiers)
    keys.push_back(kWebDriverNullKey);

  int sticky_modifiers = *modifiers;
  for (size_t i = 0; i < keys.size(); ++i) {
    char16 key = keys[i];

    if (key == kWebDriverNullKey) {
      // Release all modifier keys and clear |stick_modifiers|.
      if (sticky_modifiers & automation::kShiftKeyMask)
        key_events.push_back(CreateKeyUpEvent(ui::VKEY_SHIFT, 0));
      if (sticky_modifiers & automation::kControlKeyMask)
        key_events.push_back(CreateKeyUpEvent(ui::VKEY_CONTROL, 0));
      if (sticky_modifiers & automation::kAltKeyMask)
        key_events.push_back(CreateKeyUpEvent(ui::VKEY_MENU, 0));
      if (sticky_modifiers & automation::kMetaKeyMask)
        key_events.push_back(CreateKeyUpEvent(ui::VKEY_COMMAND, 0));
      sticky_modifiers = 0;
      continue;
    }
    if (IsModifierKey(key)) {
      // Press or release the modifier, and adjust |sticky_modifiers|.
      bool modifier_down = false;
      ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
      if (key == kWebDriverShiftKey) {
        sticky_modifiers ^= automation::kShiftKeyMask;
        modifier_down = sticky_modifiers & automation::kShiftKeyMask;
        key_code = ui::VKEY_SHIFT;
      } else if (key == kWebDriverControlKey) {
        sticky_modifiers ^= automation::kControlKeyMask;
        modifier_down = sticky_modifiers & automation::kControlKeyMask;
        key_code = ui::VKEY_CONTROL;
      } else if (key == kWebDriverAltKey) {
        sticky_modifiers ^= automation::kAltKeyMask;
        modifier_down = sticky_modifiers & automation::kAltKeyMask;
        key_code = ui::VKEY_MENU;
      } else if (key == kWebDriverCommandKey) {
        sticky_modifiers ^= automation::kMetaKeyMask;
        modifier_down = sticky_modifiers & automation::kMetaKeyMask;
        key_code = ui::VKEY_COMMAND;
      } else {
        NOTREACHED();
      }
      if (modifier_down)
        key_events.push_back(CreateKeyDownEvent(key_code, sticky_modifiers));
      else
        key_events.push_back(CreateKeyUpEvent(key_code, sticky_modifiers));
      continue;
    }

    ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
    std::string unmodified_text, modified_text;
    int all_modifiers = sticky_modifiers;

    // Get the key code, text, and modifiers for the given key.
    bool should_skip = false;
    if (KeyCodeFromSpecialWebDriverKey(key, &key_code) ||
        KeyCodeFromShorthandKey(key, &key_code, &should_skip)) {
      if (should_skip)
        continue;
      if (key_code == ui::VKEY_UNKNOWN) {
        *error_msg = base::StringPrintf(
            "Unknown WebDriver key(%d) at string index (%" PRIuS ")",
            static_cast<int>(key),
            i);
        return false;
      }
      if (key_code == ui::VKEY_RETURN) {
        // For some reason Chrome expects a carriage return for the return key.
        modified_text = unmodified_text = "\r";
      } else {
        // WebDriver assumes a numpad key should translate to the number,
        // which requires NumLock to be on with some platforms. This isn't
        // formally in the spec, but is expected by their tests.
        int webdriver_modifiers = 0;
        if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9)
          webdriver_modifiers = automation::kNumLockKeyMask;
        unmodified_text = ConvertKeyCodeToText(key_code, webdriver_modifiers);
        modified_text = ConvertKeyCodeToText(
            key_code,
            all_modifiers | webdriver_modifiers);
      }
    } else {
      int necessary_modifiers = 0;
      ConvertCharToKeyCode(key, &key_code, &necessary_modifiers);
      all_modifiers |= necessary_modifiers;
      if (key_code != ui::VKEY_UNKNOWN) {
        unmodified_text = ConvertKeyCodeToText(key_code, 0);
        modified_text = ConvertKeyCodeToText(key_code, all_modifiers);
      }
      if (unmodified_text.empty() || modified_text.empty()) {
        // Do a best effort and use the raw key we were given.
        logger.Log(
            kWarningLogLevel,
            base::StringPrintf("No translation for key code. Code point: %d",
                static_cast<int>(key)));
        if (unmodified_text.empty())
          unmodified_text = UTF16ToUTF8(keys.substr(i, 1));
        if (modified_text.empty())
          modified_text = UTF16ToUTF8(keys.substr(i, 1));
      }
    }

    // Create the key events.
    bool necessary_modifiers[3];
    for (int i = 0; i < 3; ++i) {
      necessary_modifiers[i] =
          all_modifiers & kModifiers[i].mask &&
          !(sticky_modifiers & kModifiers[i].mask);
      if (necessary_modifiers[i]) {
        key_events.push_back(
            CreateKeyDownEvent(kModifiers[i].key_code, sticky_modifiers));
      }
    }

    key_events.push_back(CreateKeyDownEvent(key_code, all_modifiers));
    if (unmodified_text.length() || modified_text.length()) {
      key_events.push_back(
          CreateCharEvent(unmodified_text, modified_text, all_modifiers));
    }
    key_events.push_back(CreateKeyUpEvent(key_code, all_modifiers));

    for (int i = 2; i > -1; --i) {
      if (necessary_modifiers[i]) {
        key_events.push_back(
            CreateKeyUpEvent(kModifiers[i].key_code, sticky_modifiers));
      }
    }
  }
  client_key_events->swap(key_events);
  *modifiers = sticky_modifiers;
  return true;
}

}  // namespace webdriver
