// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "ui/keyboard/keyboard_controller_observer.h"

namespace content {
class BrowserContext;
}

namespace keyboard {
class KeyboardController;
}

// This class connects keyboard::KeyboardControllerObserver to the
// virtualKeyboardPrivate extension API. TODO(stevenjb): For mash, this needs
// to implement a mojo interface instead of observing
// keyboard::KeyboardController directly.
class ChromeKeyboardControllerObserver
    : public keyboard::KeyboardControllerObserver {
 public:
  // |context| and |controller| must outlive this class instance.
  ChromeKeyboardControllerObserver(content::BrowserContext* context,
                                   keyboard::KeyboardController* controller);
  ~ChromeKeyboardControllerObserver() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardDisabled() override;
  void OnKeyboardConfigChanged() override;

 private:
  content::BrowserContext* const context_;
  keyboard::KeyboardController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardControllerObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_OBSERVER_H_
