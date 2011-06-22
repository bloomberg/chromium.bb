// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/models/simple_menu_model.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace gfx {
class Canvas;
}

// AvatarMenuButton
//
// A button used to show either the incognito avatar or the profile avatar.
// The button can optionally have a menu attached to it.

class AvatarMenuButton : public views::MenuButton,
                         public views::ViewMenuDelegate {
 public:
  // Creates a new button. The object will take ownership of the menu model.
  AvatarMenuButton(const std::wstring& text, ui::MenuModel* menu_model);

  virtual ~AvatarMenuButton();

  // views::MenuButton
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // views::ViewMenuDelegate
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  scoped_ptr<ui::MenuModel> menu_model_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
