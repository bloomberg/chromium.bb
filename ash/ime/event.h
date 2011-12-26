// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_EVENT_H_
#define ASH_WM_EVENT_H_
#pragma once

#include "ash/ash_export.h"
#include "base/event_types.h"
#include "ui/aura/event.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace ash {

// A key event which is translated by an input method (IME).
// For example, if an IME receives a KeyEvent(ui::VKEY_SPACE), and it does not
// consume the key, the IME usually generates and dispatches a
// TranslatedKeyEvent(ui::VKEY_SPACE) event. If the IME receives a KeyEvent and
// it does consume the event, it might dispatch a
// TranslatedKeyEvent(ui::VKEY_PROCESSKEY) event as defined in the DOM spec.
class ASH_EXPORT TranslatedKeyEvent : public aura::KeyEvent {
 public:
  TranslatedKeyEvent(const base::NativeEvent& native_event, bool is_char);

  // Used for synthetic events such as a VKEY_PROCESSKEY key event.
  TranslatedKeyEvent(bool is_press,
                     ui::KeyboardCode key_code,
                     int flags);

  // Changes the type() of the object from ET_TRANSLATED_KEY_* to ET_KEY_* so
  // that RenderWidgetHostViewAura and NativeWidgetAura could handle the event.
  void ConvertToKeyEvent();
};

}  // namespace ash

#endif  // ASH_WM_EVENT_H_
