// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_

#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace chromeos {

// Button to be used to represent status and allow menus to be popped up.
// Shows current button state by drawing a border around the current icon.
class StatusAreaButton : public views::MenuButton {
 public:
  explicit StatusAreaButton(views::ViewMenuDelegate* menu_delegate);
  virtual ~StatusAreaButton() {}
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);

 protected:
  // Draws the icon for this status area button on the canvas.
  // Subclasses should override this method if they need to draw their own icon.
  // Otherwise, just call SetIcon() and the it will be handled for you.
  virtual void DrawIcon(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(StatusAreaButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_AREA_BUTTON_H_
