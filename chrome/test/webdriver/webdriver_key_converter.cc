// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_key_converter.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/automation_constants.h"

namespace {

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

// Converts a character to the key code and modifier set that would
// produce the character using the given keyboard layout.
bool ConvertCharToKeyCode(
    char16 key, ui::KeyboardCode* key_code, int *necessary_modifiers) {
#if defined(OS_WIN)
  short vkey_and_modifiers = ::VkKeyScanW(key);
  bool translated = vkey_and_modifiers != -1 &&
                    LOBYTE(vkey_and_modifiers) != -1 &&
                    HIBYTE(vkey_and_modifiers) != -1;
  if (translated) {
    *key_code = static_cast<ui::KeyboardCode>(LOBYTE(vkey_and_modifiers));
    *necessary_modifiers = HIBYTE(vkey_and_modifiers);
  }
  return translated;
#else
  // TODO(kkania): Implement.
  return false;
#endif
}

// Returns the character that would be produced from the given key code and
// modifier set, or "" if no character would be produced.
std::string ConvertKeyCodeToText(ui::KeyboardCode key_code, int modifiers) {
#if defined(OS_WIN)
  UINT scan_code = ::MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC);
  BYTE keyboard_state[256];
  ::GetKeyboardState(keyboard_state);
  if (modifiers & automation::kShiftKeyMask)
    keyboard_state[VK_SHIFT] |= 0x80;
  wchar_t chars[5];
  int code = ::ToUnicode(key_code, scan_code, keyboard_state, chars, 4, 0);
  if (code <= 0) {
    return "";
  } else {
    std::string text;
    WideToUTF8(chars, code, &text);
    return text;
  }
#else
  // TODO(kkania): Implement
  return "";
#endif
}

}  // namespace

namespace webdriver {

WebKeyEvent CreateKeyDownEvent(ui::KeyboardCode key_code, int modifiers) {
  return WebKeyEvent(automation::kRawKeyDownType, key_code, "", "", modifiers);
}

WebKeyEvent CreateKeyUpEvent(ui::KeyboardCode key_code, int modifiers) {
  return WebKeyEvent(automation::kKeyUpType, key_code, "", "", modifiers);
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

void ConvertKeysToWebKeyEvents(const string16& client_keys,
                               std::vector<WebKeyEvent>* key_events) {
  // Add an implicit NULL character to the end of the input to depress all
  // modifiers.
  string16 keys = client_keys;
  keys.push_back(kWebDriverNullKey);

  int sticky_modifiers = 0;
  for (size_t i = 0; i < keys.size(); ++i) {
    char16 key = keys[i];

    if (key == kWebDriverNullKey) {
      // Release all modifier keys and clear |stick_modifiers|.
      if (sticky_modifiers & automation::kShiftKeyMask)
        key_events->push_back(CreateKeyUpEvent(ui::VKEY_SHIFT, 0));
      if (sticky_modifiers & automation::kControlKeyMask)
        key_events->push_back(CreateKeyUpEvent(ui::VKEY_CONTROL, 0));
      if (sticky_modifiers & automation::kAltKeyMask)
        key_events->push_back(CreateKeyUpEvent(ui::VKEY_MENU, 0));
      if (sticky_modifiers & automation::kMetaKeyMask)
        key_events->push_back(CreateKeyUpEvent(ui::VKEY_COMMAND, 0));
      sticky_modifiers = 0;
      continue;
    }
    if (IsModifierKey(key)) {
      // Press or release the modifier, and adjust |sticky_modifiers|.
      bool modifier_down = false;
      ui::KeyboardCode key_code;
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
      }
      if (modifier_down)
        key_events->push_back(CreateKeyDownEvent(key_code, sticky_modifiers));
      else
        key_events->push_back(CreateKeyUpEvent(key_code, sticky_modifiers));
      continue;
    }

    ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
    std::string unmodified_text, modified_text;
    int all_modifiers = sticky_modifiers;

    bool is_special_key = KeyCodeFromSpecialWebDriverKey(key, &key_code);
    if (is_special_key && key_code == ui::VKEY_UNKNOWN) {
      LOG(ERROR) << "Unknown WebDriver key: " << static_cast<int>(key);
      continue;
    }
    if (!is_special_key) {
      int necessary_modifiers = 0;
      ConvertCharToKeyCode(key, &key_code, &necessary_modifiers);
      all_modifiers |= necessary_modifiers;
    }
    if (key_code != ui::VKEY_UNKNOWN) {
      unmodified_text = ConvertKeyCodeToText(key_code, 0);
      modified_text = ConvertKeyCodeToText(key_code, all_modifiers);
    }
    if (!is_special_key && (unmodified_text.empty() || modified_text.empty())) {
      // Do a best effort and use the raw key we were given.
      LOG(WARNING) << "No translation for key code. Code point: "
                   << static_cast<int>(key);
      if (unmodified_text.empty())
        unmodified_text = UTF16ToUTF8(keys.substr(i, 1));
      if (modified_text.empty())
        modified_text = UTF16ToUTF8(keys.substr(i, 1));
    }

    // Create the key events.
    bool need_shift_key =
        all_modifiers & automation::kShiftKeyMask &&
        !(sticky_modifiers & automation::kShiftKeyMask);
    if (need_shift_key) {
      key_events->push_back(
          CreateKeyDownEvent(ui::VKEY_SHIFT, sticky_modifiers));
    }

    key_events->push_back(CreateKeyDownEvent(key_code, all_modifiers));
    if (unmodified_text.length() || modified_text.length()) {
      key_events->push_back(
          CreateCharEvent(unmodified_text, modified_text, all_modifiers));
    }
    key_events->push_back(CreateKeyUpEvent(key_code, all_modifiers));

    if (need_shift_key) {
      key_events->push_back(
          CreateKeyUpEvent(ui::VKEY_SHIFT, sticky_modifiers));
    }
  }
}

}  // namespace webdriver
