// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_KEYBOARD_DRIVEN_OOBE_KEY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_KEYBOARD_DRIVEN_OOBE_KEY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/events/event_handler.h"

namespace chromeos {

// A class to handle special menu key for keyboard driven OOBE.
class KeyboardDrivenOobeKeyHandler : public ui::EventHandler {
 public:
  KeyboardDrivenOobeKeyHandler();
  virtual ~KeyboardDrivenOobeKeyHandler();

 private:
  // ui::EventHandler
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenOobeKeyHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_KEYBOARD_DRIVEN_OOBE_KEY_HANDLER_H_
