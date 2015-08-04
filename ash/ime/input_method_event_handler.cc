// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/input_method_event_handler.h"

#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventHandler, public:

InputMethodEventHandler::InputMethodEventHandler(ui::InputMethod* input_method)
    : input_method_(input_method), post_ime_(false) {
}

InputMethodEventHandler::~InputMethodEventHandler() {
}

void InputMethodEventHandler::SetPostIME(bool post_ime) {
  post_ime_ = post_ime;
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventHandler, EventFilter implementation:

void InputMethodEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  // Never send synthesized key event.
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  if (post_ime_)
    return;
  input_method_->DispatchKeyEvent(event);
  event->StopPropagation();
}

}  // namespace ash
