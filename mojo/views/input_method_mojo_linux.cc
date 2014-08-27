// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/views/input_method_mojo_linux.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace mojo {

InputMethodMojoLinux::InputMethodMojoLinux(
    ui::internal::InputMethodDelegate* delegate)
    : ui::InputMethodAuraLinux(delegate) {
}

InputMethodMojoLinux::~InputMethodMojoLinux() {}

bool InputMethodMojoLinux::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ui::ET_KEY_PRESSED ||
         event.type() == ui::ET_KEY_RELEASED);
  DCHECK(system_toplevel_window_focused());

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  // Here is where we change the differ from our base class's logic. Instead of
  // always dispatching a key down event, and then sending a synthesized
  // character event, we instead check to see if this is a character event and
  // send out the key if it is. (We fallback to normal dispatch if it isn't.)
  if (event.is_char()) {
    const uint16 ch = event.GetCharacter();
    if (GetTextInputClient())
      GetTextInputClient()->InsertChar(ch, event.flags());

    return false;
  }

  return DispatchKeyEventPostIME(event);
}

}  // namespace mojo
