// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_MANAGER_H_
#define CHROME_BROWSER_UI_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_MANAGER_H_
#pragma once

#include "base/memory/singleton.h"
#include "ui/views/desktop/desktop_window_view.h"
#include "views/widget/widget.h"

class KeyboardWidget;

// A singleton object to manage the virtual keyboard.
class VirtualKeyboardManager : public views::Widget::Observer,
                        public views::desktop::DesktopWindowView::Observer {
 public:
  // Returns the singleton object.
  static VirtualKeyboardManager* GetInstance();

  // Shows the keyboard for the target widget. The events from the keyboard will
  // be sent to |widget|.
  // TODO(sad): Allow specifying the type of keyboard to show.
  void ShowKeyboardForWidget(views::Widget* widget);

  // Hides the keyboard.
  void Hide();

  // Returns the keyboard Widget.
  views::Widget* keyboard();

 private:
  friend struct DefaultSingletonTraits<VirtualKeyboardManager>;

  VirtualKeyboardManager();
  virtual ~VirtualKeyboardManager();

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Overridden from views::desktop::DesktopWindowView::Observer
  virtual void OnDesktopBoundsChanged(const gfx::Rect& prev_bounds) OVERRIDE;

  KeyboardWidget* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardManager);
};

#endif  // CHROME_BROWSER_UI_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_MANAGER_H_
