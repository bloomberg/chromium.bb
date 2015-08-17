// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/input_method_mandoline.h"

#include "components/view_manager/public/cpp/view.h"
#include "mojo/converters/ime/ime_type_converters.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/mojo/ime/text_input_state.mojom.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMandoline, public:

InputMethodMandoline::InputMethodMandoline(
    ui::internal::InputMethodDelegate* delegate,
    mojo::View* view)
    : view_(view) {
  SetDelegate(delegate);
}

InputMethodMandoline::~InputMethodMandoline() {}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMandoline, ui::InputMethod implementation:

void InputMethodMandoline::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateTextInputType();
}

void InputMethodMandoline::OnBlur() {
  InputMethodBase::OnBlur();
  UpdateTextInputType();
}

bool InputMethodMandoline::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

void InputMethodMandoline::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  // Here is where we change the differ from our base class's logic. Instead of
  // always dispatching a key down event, and then sending a synthesized
  // character event, we instead check to see if this is a character event and
  // send out the key if it is. (We fallback to normal dispatch if it isn't.)
  if (event->is_char()) {
    GetTextInputClient()->InsertChar(event->GetCharacter(), event->flags());
    event->StopPropagation();
    return;
  }

  ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodMandoline::OnTextInputTypeChanged(
    const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    UpdateTextInputType();
  InputMethodBase::OnTextInputTypeChanged(client);
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

bool InputMethodMandoline::IsCandidatePopupOpen() const {
  return false;
}

void InputMethodMandoline::OnDidChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
  UpdateTextInputType();
}

void InputMethodMandoline::UpdateTextInputType() {
  ui::TextInputType type = GetTextInputType();
  mojo::TextInputStatePtr state = mojo::TextInputState::New();
  state->type = mojo::ConvertTo<mojo::TextInputType>(type);
  if (type != ui::TEXT_INPUT_TYPE_NONE)
    view_->SetImeVisibility(true, state.Pass());
  else
    view_->SetTextInputState(state.Pass());
}

}  // namespace mandoline
