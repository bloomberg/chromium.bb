// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
#pragma once

#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/views/view.h"

namespace views {
class FocusManager;
}  // namespace views

// This class handles unhandled keyboard messages coming back from the renderer
// process.
class UnhandledKeyboardEventHandler {
 public:
  UnhandledKeyboardEventHandler();
  ~UnhandledKeyboardEventHandler();

  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                           views::FocusManager* focus_manager);

 private:
  // Whether to ignore the next Char keyboard event.
  // If a RawKeyDown event was handled as a shortcut key, then we're done
  // handling it and should eat any Char event that the translate phase may
  // have produced from it. (Handling this event may cause undesirable effects,
  // such as a beep if DefWindowProc() has no default handling for the given
  // Char.)
  bool ignore_next_char_event_;

  DISALLOW_COPY_AND_ASSIGN(UnhandledKeyboardEventHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
