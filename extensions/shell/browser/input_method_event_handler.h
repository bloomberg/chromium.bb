// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_INPUT_METHOD_EVENT_HANDLER_H_
#define EXTENSIONS_SHELL_BROWSER_INPUT_METHOD_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ui {
class InputMethod;
class KeyEvent;
}

namespace extensions {

// Basic EventHandler for dispatching key events.
class InputMethodEventHandler : public ui::EventHandler {
 public:
  explicit InputMethodEventHandler(ui::InputMethod* input_method);
  ~InputMethodEventHandler() override;

  void set_post_ime(bool post_ime);

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // The input method to pass events. Should outlive this handler.
  ui::InputMethod* input_method_;

  // True when an event is dispatched within DispatchKeyEventPostIME(), meaning
  // it has already been processed by the input method.
  bool post_ime_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEventHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_INPUT_METHOD_EVENT_HANDLER_H_
