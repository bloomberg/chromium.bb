// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/keymap.h"

#include "chrome/browser/automation/ui_controls.h"
#include "views/event.h"

namespace webdriver {

KeyMap::KeyMap() {
  // These are keys stored in the Unicode PUA (Private Use Area) code points,
  // 0xE000-0xF8FF.

  // Special WebDriver NULL key; clears all modifiers.
  keys_[L'\uE000'] = base::VKEY_UNKNOWN;

  keys_[L'\uE001'] = base::VKEY_UNKNOWN;  // TODO(jmikhail): CANCEL
  keys_[L'\uE002'] = base::VKEY_HELP;
  keys_[L'\uE003'] = base::VKEY_BACK;     // BACKSPACE
  keys_[L'\uE004'] = base::VKEY_TAB;
  keys_[L'\uE005'] = base::VKEY_CLEAR;
  keys_[L'\uE006'] = base::VKEY_RETURN;
  keys_[L'\uE007'] = base::VKEY_UNKNOWN;  // TODO(jmikhail): ENTER
  keys_[L'\uE008'] = base::VKEY_SHIFT;
  keys_[L'\uE009'] = base::VKEY_CONTROL;
  keys_[L'\uE00A'] = base::VKEY_MENU;     // ALT
  keys_[L'\uE00B'] = base::VKEY_PAUSE;
  keys_[L'\uE00C'] = base::VKEY_ESCAPE;
  keys_[L'\uE00D'] = base::VKEY_SPACE;
  keys_[L'\uE00E'] = base::VKEY_PRIOR;    // PAGEUP
  keys_[L'\uE00F'] = base::VKEY_NEXT;     // PAGEDOWN
  keys_[L'\uE010'] = base::VKEY_END;
  keys_[L'\uE011'] = base::VKEY_HOME;
  keys_[L'\uE012'] = base::VKEY_LEFT;
  keys_[L'\uE013'] = base::VKEY_UP;
  keys_[L'\uE014'] = base::VKEY_RIGHT;
  keys_[L'\uE015'] = base::VKEY_DOWN;
  keys_[L'\uE016'] = base::VKEY_INSERT;
  keys_[L'\uE017'] = base::VKEY_DELETE;

  keys_[L'\uE01A'] = base::VKEY_NUMPAD0;
  keys_[L'\uE01B'] = base::VKEY_NUMPAD1;
  keys_[L'\uE01C'] = base::VKEY_NUMPAD2;
  keys_[L'\uE01D'] = base::VKEY_NUMPAD3;
  keys_[L'\uE01E'] = base::VKEY_NUMPAD4;
  keys_[L'\uE01F'] = base::VKEY_NUMPAD5;
  keys_[L'\uE020'] = base::VKEY_NUMPAD6;
  keys_[L'\uE021'] = base::VKEY_NUMPAD7;
  keys_[L'\uE022'] = base::VKEY_NUMPAD8;
  keys_[L'\uE023'] = base::VKEY_NUMPAD9;
  keys_[L'\uE024'] = base::VKEY_MULTIPLY;
  keys_[L'\uE025'] = base::VKEY_ADD;
  keys_[L'\uE026'] = base::VKEY_SEPARATOR;
  keys_[L'\uE027'] = base::VKEY_SUBTRACT;
  keys_[L'\uE028'] = base::VKEY_DECIMAL;
  keys_[L'\uE029'] = base::VKEY_DIVIDE;

  keys_[L'\uE031'] = base::VKEY_F1;
  keys_[L'\uE032'] = base::VKEY_F2;
  keys_[L'\uE033'] = base::VKEY_F3;
  keys_[L'\uE034'] = base::VKEY_F4;
  keys_[L'\uE035'] = base::VKEY_F5;
  keys_[L'\uE036'] = base::VKEY_F6;
  keys_[L'\uE037'] = base::VKEY_F7;
  keys_[L'\uE038'] = base::VKEY_F8;
  keys_[L'\uE039'] = base::VKEY_F9;
  keys_[L'\uE03A'] = base::VKEY_F10;
  keys_[L'\uE03B'] = base::VKEY_F11;
  keys_[L'\uE03C'] = base::VKEY_F12;

  // Common aliases.
  keys_[L'\t'] = base::VKEY_TAB;
  keys_[L'\n'] = base::VKEY_RETURN;
  keys_[L'\r'] = base::VKEY_RETURN;
  keys_[L'\b'] = base::VKEY_BACK;

  keys_[L' '] = base::VKEY_SPACE;

  // Alpha keys match their ASCII values
  for (int i = 0; i < 26; ++i) {
    keys_[static_cast<wchar_t>(L'a' + i)] = \
      static_cast<base::KeyboardCode>(base::VKEY_A + i);
    shifted_keys_[static_cast<wchar_t>(L'A' + i)] = \
      static_cast<base::KeyboardCode>(base::VKEY_A + i);
  }

  // Numeric keys match their ASCII values
  for (int i = 0; i < 10; ++i) {
    keys_[static_cast<wchar_t>(L'0' + i)] = \
      static_cast<base::KeyboardCode>(base::VKEY_0 + i);
  }

  // The following all assumes the standard US keyboard.
  // TODO(jmikhail): Lookup correct keycode based on the current system keyboard
  // layout.  Right now it's fixed assuming standard ANSI
  keys_[L'=']  = shifted_keys_[L'+'] = base::VKEY_OEM_PLUS;
  keys_[L'-']  = shifted_keys_[L'_'] = base::VKEY_OEM_MINUS;
  keys_[L';']  = shifted_keys_[L':'] = base::VKEY_OEM_1;
  keys_[L'/']  = shifted_keys_[L'?'] = base::VKEY_OEM_2;
  keys_[L'`']  = shifted_keys_[L'~'] = base::VKEY_OEM_3;
  keys_[L'[']  = shifted_keys_[L'{'] = base::VKEY_OEM_4;
  keys_[L'\\'] = shifted_keys_[L'|'] = base::VKEY_OEM_5;
  keys_[L']']  = shifted_keys_[L'}'] = base::VKEY_OEM_6;
  keys_[L'\''] = shifted_keys_[L'"'] = base::VKEY_OEM_7;
  keys_[L',']  = shifted_keys_[L'<'] = base::VKEY_OEM_COMMA;
  keys_[L'.']  = shifted_keys_[L'>'] = base::VKEY_OEM_PERIOD;
  shifted_keys_[L'!'] = base::VKEY_1;
  shifted_keys_[L'@'] = base::VKEY_2;
  shifted_keys_[L'#'] = base::VKEY_3;
  shifted_keys_[L'$'] = base::VKEY_4;
  shifted_keys_[L'%'] = base::VKEY_5;
  shifted_keys_[L'^'] = base::VKEY_6;
  shifted_keys_[L'&'] = base::VKEY_7;
  shifted_keys_[L'*'] = base::VKEY_8;
  shifted_keys_[L'('] = base::VKEY_9;
  shifted_keys_[L')'] = base::VKEY_0;
}

base::KeyboardCode KeyMap::Get(const wchar_t& key) const {
  std::map<wchar_t, base::KeyboardCode>::const_iterator it;
  it = keys_.find(key);
  if (it == keys_.end()) {
    it = shifted_keys_.find(key);
    if (it == shifted_keys_.end()) {
      return base::VKEY_UNKNOWN;
    }
  }
  return it->second;
}

bool KeyMap::Press(const scoped_refptr<WindowProxy>& window,
                   const base::KeyboardCode key_code,
                   const wchar_t& key) {
  if (key_code == base::VKEY_SHIFT) {
    shift_ = !shift_;
  } else if (key_code == base::VKEY_CONTROL) {
    control_ = !control_;
  } else if (key_code == base::VKEY_MENU) {  // ALT
    alt_ = !alt_;
  } else if (key_code == base::VKEY_COMMAND) {
    command_ = !command_;
  }

  int modifiers = 0;
  if (shift_ || shifted_keys_.find(key) != shifted_keys_.end()) {
    modifiers = modifiers | views::Event::EF_SHIFT_DOWN;
  }
  if (control_) {
    modifiers = modifiers | views::Event::EF_CONTROL_DOWN;
  }
  if (alt_) {
    modifiers = modifiers | views::Event::EF_ALT_DOWN;
  }
  if (command_) {
    LOG(INFO) << "Pressing command key on linux!!";
    modifiers = modifiers | views::Event::EF_COMMAND_DOWN;
  }

  // TODO(jmikhail): need to be able to capture modifier key up
  window->SimulateOSKeyPress(key_code, modifiers);

  return true;
}

void KeyMap::ClearModifiers() {
  shift_ = false;
  alt_ = false;
  control_ = false;
  command_ = false;
}
}  // namespace webdriver

