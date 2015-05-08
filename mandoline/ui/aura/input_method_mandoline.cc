// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/input_method_mandoline.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMandoline, public:

InputMethodMandoline::InputMethodMandoline(
    ui::internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodMandoline::~InputMethodMandoline() {}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMandoline, ui::InputMethod implementation:

bool InputMethodMandoline::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

bool InputMethodMandoline::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ui::ET_KEY_PRESSED ||
         event.type() == ui::ET_KEY_RELEASED);

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

void InputMethodMandoline::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
}

void InputMethodMandoline::CancelComposition(
    const ui::TextInputClient* client) {
}

void InputMethodMandoline::OnInputLocaleChanged() {
}

std::string InputMethodMandoline::GetInputLocale() {
  return "";
}

bool InputMethodMandoline::IsActive() {
  return true;
}

bool InputMethodMandoline::IsCandidatePopupOpen() const {
  return false;
}

}  // namespace mandoline
