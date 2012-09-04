// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu_button_win.h"

#include "ui/base/events/event.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/views/widget/widget.h"

AppMenuButtonWin::AppMenuButtonWin(views::MenuButtonListener* listener)
    : views::MenuButton(NULL, string16(), listener, false) {
}

bool AppMenuButtonWin::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE) {
    // Typical windows behavior is to show the system menu on space.
    views::Widget* widget = GetWidget();
    gfx::Rect bounds = widget->GetClientAreaBoundsInScreen();
    ui::ShowSystemMenu(widget->GetNativeView(), bounds.x(), bounds.y() + 10);
    return false;
  }
  return views::MenuButton::OnKeyPressed(event);
}
