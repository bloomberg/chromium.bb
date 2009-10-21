// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_

#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"

// Button to be used to represent status and allow menus to be popped up.
// Shows current button state by drawing a border around the current icon.
class StatusAreaButton : public views::MenuButton {
 public:
  StatusAreaButton(views::ViewMenuDelegate* menu_delegate);
  virtual ~StatusAreaButton() {}
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_
