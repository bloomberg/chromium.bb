// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_INPUT_METHOD_EVENT_HANDLER_H_
#define ASH_IME_INPUT_METHOD_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace ui {
class InputMethod;
class KeyEvent;
}

namespace ash {

// An EventHandler
class ASH_EXPORT InputMethodEventHandler : public ui::EventHandler {
 public:
  explicit InputMethodEventHandler(ui::InputMethod* input_method);
  ~InputMethodEventHandler() override;

  // True to skip sending event to the InputMethod. This is used
  // to process event passed from |DispatchKeyEventPostIME|.
  void SetPostIME(bool post_ime);

 private:
  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  ui::InputMethod* input_method_;

  bool post_ime_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEventHandler);
};

}  // namespace ash

#endif  // ASH_INPUT_METHOD_EVENT_HANDLER_H_
