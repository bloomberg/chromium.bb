// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/view_menu_delegate.h"

namespace gfx {
class Canvas;
class Image;
}
class Browser;

// AvatarMenuButton
//
// A button used to show either the incognito avatar or the profile avatar.
// The button can optionally have a menu attached to it.

class AvatarMenuButton : public views::MenuButton,
                         public views::ViewMenuDelegate {
 public:
  // Creates a new button. If |has_menu| is true then clicking on the button
  // will cause the profile menu to be displayed.
  AvatarMenuButton(Browser* browser, bool has_menu);

  virtual ~AvatarMenuButton();

  // views::MenuButton
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool HitTest(const gfx::Point& point) const OVERRIDE;

  virtual void SetAvatarIcon(const gfx::Image& icon,
                       bool is_gaia_picture);

  void ShowAvatarBubble();

 private:
  // views::ViewMenuDelegate
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  Browser* browser_;
  bool has_menu_;
  bool set_taskbar_decoration_;
  scoped_ptr<ui::MenuModel> menu_model_;

  // Use a scoped ptr because gfx::Image doesn't have a default constructor.
  scoped_ptr<gfx::Image> icon_;
  SkBitmap button_icon_;
  bool is_gaia_picture_;
  int old_height_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
