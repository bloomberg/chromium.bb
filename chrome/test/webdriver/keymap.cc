// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/keymap.h"

#include "chrome/browser/automation/ui_controls.h"
#include "views/events/event.h"

namespace webdriver {

KeyMap::KeyMap() {
  // These are keys stored in the Unicode PUA (Private Use Area) code points,
  // 0xE000-0xF8FF.

  // Special WebDriver NULL key; clears all modifiers.
  keys_[L'\uE000'] = ui::VKEY_UNKNOWN;

  keys_[L'\uE001'] = ui::VKEY_UNKNOWN;  // TODO(jmikhail): CANCEL
  keys_[L'\uE002'] = ui::VKEY_HELP;
  keys_[L'\uE003'] = ui::VKEY_BACK;     // BACKSPACE
  keys_[L'\uE004'] = ui::VKEY_TAB;
  keys_[L'\uE005'] = ui::VKEY_CLEAR;
  keys_[L'\uE006'] = ui::VKEY_RETURN;
  keys_[L'\uE007'] = ui::VKEY_UNKNOWN;  // TODO(jmikhail): ENTER
  keys_[L'\uE008'] = ui::VKEY_SHIFT;
  keys_[L'\uE009'] = ui::VKEY_CONTROL;
  keys_[L'\uE00A'] = ui::VKEY_MENU;     // ALT
  keys_[L'\uE00B'] = ui::VKEY_PAUSE;
  keys_[L'\uE00C'] = ui::VKEY_ESCAPE;
  keys_[L'\uE00D'] = ui::VKEY_SPACE;
  keys_[L'\uE00E'] = ui::VKEY_PRIOR;    // PAGEUP
  keys_[L'\uE00F'] = ui::VKEY_NEXT;     // PAGEDOWN
  keys_[L'\uE010'] = ui::VKEY_END;
  keys_[L'\uE011'] = ui::VKEY_HOME;
  keys_[L'\uE012'] = ui::VKEY_LEFT;
  keys_[L'\uE013'] = ui::VKEY_UP;
  keys_[L'\uE014'] = ui::VKEY_RIGHT;
  keys_[L'\uE015'] = ui::VKEY_DOWN;
  keys_[L'\uE016'] = ui::VKEY_INSERT;
  keys_[L'\uE017'] = ui::VKEY_DELETE;

  keys_[L'\uE01A'] = ui::VKEY_NUMPAD0;
  keys_[L'\uE01B'] = ui::VKEY_NUMPAD1;
  keys_[L'\uE01C'] = ui::VKEY_NUMPAD2;
  keys_[L'\uE01D'] = ui::VKEY_NUMPAD3;
  keys_[L'\uE01E'] = ui::VKEY_NUMPAD4;
  keys_[L'\uE01F'] = ui::VKEY_NUMPAD5;
  keys_[L'\uE020'] = ui::VKEY_NUMPAD6;
  keys_[L'\uE021'] = ui::VKEY_NUMPAD7;
  keys_[L'\uE022'] = ui::VKEY_NUMPAD8;
  keys_[L'\uE023'] = ui::VKEY_NUMPAD9;
  keys_[L'\uE024'] = ui::VKEY_MULTIPLY;
  keys_[L'\uE025'] = ui::VKEY_ADD;
  keys_[L'\uE026'] = ui::VKEY_SEPARATOR;
  keys_[L'\uE027'] = ui::VKEY_SUBTRACT;
  keys_[L'\uE028'] = ui::VKEY_DECIMAL;
  keys_[L'\uE029'] = ui::VKEY_DIVIDE;

  keys_[L'\uE031'] = ui::VKEY_F1;
  keys_[L'\uE032'] = ui::VKEY_F2;
  keys_[L'\uE033'] = ui::VKEY_F3;
  keys_[L'\uE034'] = ui::VKEY_F4;
  keys_[L'\uE035'] = ui::VKEY_F5;
  keys_[L'\uE036'] = ui::VKEY_F6;
  keys_[L'\uE037'] = ui::VKEY_F7;
  keys_[L'\uE038'] = ui::VKEY_F8;
  keys_[L'\uE039'] = ui::VKEY_F9;
  keys_[L'\uE03A'] = ui::VKEY_F10;
  keys_[L'\uE03B'] = ui::VKEY_F11;
  keys_[L'\uE03C'] = ui::VKEY_F12;

  // Common aliases.
  keys_[L'\t'] = ui::VKEY_TAB;
  keys_[L'\n'] = ui::VKEY_RETURN;
  keys_[L'\r'] = ui::VKEY_RETURN;
  keys_[L'\b'] = ui::VKEY_BACK;

  keys_[L' '] = ui::VKEY_SPACE;

  // Alpha keys match their ASCII values.
  for (int i = 0; i < 26; ++i) {
    keys_[static_cast<wchar_t>(L'a' + i)] = \
      static_cast<ui::KeyboardCode>(ui::VKEY_A + i);
    shifted_keys_[static_cast<wchar_t>(L'A' + i)] = \
      static_cast<ui::KeyboardCode>(ui::VKEY_A + i);
  }

  // Numeric keys match their ASCII values.
  for (int i = 0; i < 10; ++i) {
    keys_[static_cast<wchar_t>(L'0' + i)] = \
      static_cast<ui::KeyboardCode>(ui::VKEY_0 + i);
  }

  // The following all assumes the standard US keyboard.
  // TODO(jmikhail): Lookup correct keycode based on the current system keyboard
  // layout.  Right now it's fixed assuming standard ANSI.
  keys_[L'=']  = shifted_keys_[L'+'] = ui::VKEY_OEM_PLUS;
  keys_[L'-']  = shifted_keys_[L'_'] = ui::VKEY_OEM_MINUS;
  keys_[L';']  = shifted_keys_[L':'] = ui::VKEY_OEM_1;
  keys_[L'/']  = shifted_keys_[L'?'] = ui::VKEY_OEM_2;
  keys_[L'`']  = shifted_keys_[L'~'] = ui::VKEY_OEM_3;
  keys_[L'[']  = shifted_keys_[L'{'] = ui::VKEY_OEM_4;
  keys_[L'\\'] = shifted_keys_[L'|'] = ui::VKEY_OEM_5;
  keys_[L']']  = shifted_keys_[L'}'] = ui::VKEY_OEM_6;
  keys_[L'\''] = shifted_keys_[L'"'] = ui::VKEY_OEM_7;
  keys_[L',']  = shifted_keys_[L'<'] = ui::VKEY_OEM_COMMA;
  keys_[L'.']  = shifted_keys_[L'>'] = ui::VKEY_OEM_PERIOD;
  shifted_keys_[L'!'] = ui::VKEY_1;
  shifted_keys_[L'@'] = ui::VKEY_2;
  shifted_keys_[L'#'] = ui::VKEY_3;
  shifted_keys_[L'$'] = ui::VKEY_4;
  shifted_keys_[L'%'] = ui::VKEY_5;
  shifted_keys_[L'^'] = ui::VKEY_6;
  shifted_keys_[L'&'] = ui::VKEY_7;
  shifted_keys_[L'*'] = ui::VKEY_8;
  shifted_keys_[L'('] = ui::VKEY_9;
  shifted_keys_[L')'] = ui::VKEY_0;
}

KeyMap::~KeyMap() {}

ui::KeyboardCode KeyMap::Get(const wchar_t& key) const {
  std::map<wchar_t, ui::KeyboardCode>::const_iterator it;
  it = keys_.find(key);
  if (it == keys_.end()) {
    it = shifted_keys_.find(key);
    if (it == shifted_keys_.end()) {
      return ui::VKEY_UNKNOWN;
    }
  }
  return it->second;
}

bool KeyMap::Press(const scoped_refptr<WindowProxy>& window,
                   const ui::KeyboardCode key_code,
                   const wchar_t& key) {
  if (key_code == ui::VKEY_SHIFT) {
    shift_ = !shift_;
  } else if (key_code == ui::VKEY_CONTROL) {
    control_ = !control_;
  } else if (key_code == ui::VKEY_MENU) {  // ALT
    alt_ = !alt_;
  } else if (key_code == ui::VKEY_COMMAND) {
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
    VLOG(1) << "Pressing command key on linux!!";
    modifiers = modifiers | views::Event::EF_COMMAND_DOWN;
  }

  // TODO(jmikhail): need to be able to capture modifier key up.
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

