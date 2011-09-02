// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/root_window.h"

#include "base/logging.h"
#include "aura/event.h"
#include "aura/window_delegate.h"
#include "ui/base/events.h"

namespace aura {
namespace internal {

RootWindow::RootWindow() : Window(NULL), mouse_pressed_handler_(NULL) {
}

RootWindow::~RootWindow() {
}

bool RootWindow::HandleMouseEvent(const MouseEvent& event) {
  Window* target = mouse_pressed_handler_;
  if (!target)
    target = GetEventHandlerForPoint(event.location());
  if (event.type() == ui::ET_MOUSE_PRESSED && !mouse_pressed_handler_)
    mouse_pressed_handler_ = target;
  if (event.type() == ui::ET_MOUSE_RELEASED)
    mouse_pressed_handler_ = NULL;
  if (target->delegate()) {
    MouseEvent translated_event(event, this, target);
    return target->OnMouseEvent(&translated_event);
  }
  return false;
}

}  // namespace internal
}  // namespace aura
