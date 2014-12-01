// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_KEYBOARD_HOOK_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_KEYBOARD_HOOK_HANDLER_H_

#include "ui/views/widget/widget.h"

// Interface to control keyboard hook on different platform.
class KeyboardHookHandler {
 public:
  // Request all keyboard events to be routed to the given window.
  void Register(views::Widget* widget);

  // Release the request for all keyboard events.
  void Deregister(views::Widget* widget);

  // Implemented in platform specific code.
  static KeyboardHookHandler* GetInstance();
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_KEYBOARD_HOOK_HANDLER_H_
