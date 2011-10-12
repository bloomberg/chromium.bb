// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu_button_win.h"

#include "ui/base/win/hwnd_util.h"
#include "views/widget/widget.h"

AppMenuButtonWin::AppMenuButtonWin(views::ViewMenuDelegate* menu_delegate)
    : views::MenuButton(NULL, string16(), menu_delegate, false) {
}

bool AppMenuButtonWin::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE) {
    // Typical windows behavior is to show the system menu on space.
    views::Widget* widget = GetWidget();
    gfx::Rect bounds = widget->GetClientAreaScreenBounds();
    ui::ShowSystemMenu(widget->GetNativeView(), bounds.x(), bounds.y() + 10);
    return false;
  }
  return views::MenuButton::OnKeyPressed(event);
}
