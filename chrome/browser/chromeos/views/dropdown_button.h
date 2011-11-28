// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_DROPDOWN_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_VIEWS_DROPDOWN_BUTTON_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/views/controls/button/menu_button.h"

namespace chromeos {

// MenuButton specification that uses different icons to draw the button and
// adjust focus frame accordingly to the icons particularities.
class DropDownButton : public views::MenuButton {
 public:
  DropDownButton(views::ButtonListener* listener,
                 const string16& text,
                 views::ViewMenuDelegate* menu_delegate,
                 bool show_menu_marker);
  virtual ~DropDownButton();

  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Override SetText to set the accessible value, rather than the
  // accessible name, since this acts more like a combo box than a
  // menu.
  virtual void SetText(const string16& text) OVERRIDE;

  virtual string16 GetAccessibleValue();

 private:
  DISALLOW_COPY_AND_ASSIGN(DropDownButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VIEWS_DROPDOWN_BUTTON_H_
