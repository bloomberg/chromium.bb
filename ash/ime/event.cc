// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/event.h"

namespace ash {

TranslatedKeyEvent::TranslatedKeyEvent(const base::NativeEvent& native_event,
                                       bool is_char)
    : KeyEvent(native_event, is_char) {
  set_type(type() == ui::ET_KEY_PRESSED ?
           ui::ET_TRANSLATED_KEY_PRESS : ui::ET_TRANSLATED_KEY_RELEASE);
}

TranslatedKeyEvent::TranslatedKeyEvent(bool is_press,
                                       ui::KeyboardCode key_code,
                                       int flags)
    : KeyEvent((is_press ?
                ui::ET_TRANSLATED_KEY_PRESS : ui::ET_TRANSLATED_KEY_RELEASE),
               key_code,
               flags) {
}

void TranslatedKeyEvent::ConvertToKeyEvent() {
  set_type(type() == ui::ET_TRANSLATED_KEY_PRESS ?
           ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED);
}

}  // namespace ash
