// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_BUTTON_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_BUTTON_WIN_H_
#pragma once

#include "ui/views/controls/button/menu_button.h"

// Button used for showing the app menu. Special cases space to show the system
// menu.
class AppMenuButtonWin : public views::MenuButton {
 public:
  explicit AppMenuButtonWin(views::ViewMenuDelegate* menu_delegate);

  // MenuButton:
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppMenuButtonWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_BUTTON_WIN_H_
