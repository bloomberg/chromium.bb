// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/input_method_event_handler.h"

#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"

namespace extensions {

InputMethodEventHandler::InputMethodEventHandler(ui::InputMethod* input_method)
    : input_method_(input_method), post_ime_(false) {
  DCHECK(input_method_);
}

InputMethodEventHandler::~InputMethodEventHandler() {}

void InputMethodEventHandler::set_post_ime(bool post_ime) {
  post_ime_ = post_ime;
}

void InputMethodEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore events being dispatched back to the delegate from the input method.
  if (post_ime_)
    return;

  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  input_method_->DispatchKeyEvent(event);
  event->StopPropagation();
}

}  // namespace extensions
