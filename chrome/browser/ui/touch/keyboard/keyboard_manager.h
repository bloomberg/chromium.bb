// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
#define CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
#pragma once

#include "base/memory/singleton.h"
#include "views/widget/widget.h"

class KeyboardWidget;

// A singleton object to manage the virtual keyboard.
class KeyboardManager : public views::Widget::Observer {
 public:
  // Returns the singleton object.
  static KeyboardManager* GetInstance();

  // Shows the keyboard for the target widget. The events from the keyboard will
  // be sent to |widget|.
  // TODO(sad): Allow specifying the type of keyboard to show.
  void ShowKeyboardForWidget(views::Widget* widget);

  // Hides the keyboard.
  void Hide();

 private:
  friend struct DefaultSingletonTraits<KeyboardManager>;

  KeyboardManager();
  virtual ~KeyboardManager();

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  KeyboardWidget* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardManager);
};

#endif  // CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
