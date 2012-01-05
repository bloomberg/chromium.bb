// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"

#include "base/logging.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/aura/event.h"
#include "ui/views/focus/focus_manager.h"

UnhandledKeyboardEventHandler::UnhandledKeyboardEventHandler() {
}

void UnhandledKeyboardEventHandler::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    views::FocusManager* focus_manager) {
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  // A false char event may be sent to the renderer if the key
  // event changed the focus. See crbug.com/108480.
  if (event.os_event && !event.skip_in_browser &&
      !static_cast<aura::KeyEvent*>(event.os_event)->is_char()) {
    views::KeyEvent views_event(event.os_event);
    focus_manager->OnKeyEvent(views_event);
  }
}
