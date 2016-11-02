// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/ui_events.h"

#include "base/logging.h"

MouseEvent::MouseEvent(MouseEventType type,
                       MouseButton button,
                       int x,
                       int y,
                       int modifiers,
                       int click_count)
    : type(type),
      button(button),
      x(x),
      y(y),
      modifiers(modifiers),
      click_count(click_count) {}

MouseEvent::~MouseEvent() {}

TouchEvent::TouchEvent(TouchEventType type,
                       int x,
                       int y)
    : type(type),
      x(x),
      y(y) {}

TouchEvent::~TouchEvent() {}

KeyEvent::KeyEvent(KeyEventType type,
                   int modifiers,
                   const std::string& modified_text,
                   const std::string& unmodified_text,
                   ui::KeyboardCode key_code)
    : type(type),
      modifiers(modifiers),
      modified_text(modified_text),
      unmodified_text(unmodified_text),
      key_code(key_code) {}

KeyEvent::~KeyEvent() {}

KeyEventBuilder::KeyEventBuilder()
    : key_event_(kInvalidEventType,
                 0,
                 std::string(),
                 std::string(),
                 ui::VKEY_UNKNOWN) {}

KeyEventBuilder::~KeyEventBuilder() {}

KeyEventBuilder* KeyEventBuilder::SetType(KeyEventType type) {
  key_event_.type = type;
  return this;
}

KeyEventBuilder* KeyEventBuilder::AddModifiers(int modifiers) {
  key_event_.modifiers |= modifiers;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetModifiers(int modifiers) {
  key_event_.modifiers = modifiers;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetText(const std::string& unmodified_text,
                                          const std::string& modified_text) {
  key_event_.unmodified_text = unmodified_text;
  key_event_.modified_text = modified_text;
  return this;
}

KeyEventBuilder* KeyEventBuilder::SetKeyCode(ui::KeyboardCode key_code) {
  key_event_.key_code = key_code;
  return this;
}

KeyEvent KeyEventBuilder::Build() {
  DCHECK(key_event_.type != kInvalidEventType);
  return key_event_;
}

void KeyEventBuilder::Generate(std::list<KeyEvent>* key_events) {
  key_events->push_back(SetType(kRawKeyDownEventType)->Build());
  if (key_event_.modified_text.length() || key_event_.unmodified_text.length())
    key_events->push_back(SetType(kCharEventType)->Build());
  key_events->push_back(SetType(kKeyUpEventType)->Build());
}
