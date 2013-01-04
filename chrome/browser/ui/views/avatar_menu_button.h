// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

namespace gfx {
class Canvas;
class Image;
}
class Browser;

// Draws a scaled version of the avatar in |image| on the taskbar button
// associated with top level, visible |window|. Currently only implemented
// for Windows 7 and above.
void DrawTaskBarDecoration(gfx::NativeWindow window, const gfx::Image* image);

// AvatarMenuButton
//
// A button used to show either the incognito avatar or the profile avatar.
// The button can optionally have a menu attached to it.

class AvatarMenuButton : public views::MenuButton,
                         public views::MenuButtonListener {
 public:
  // Creates a new button. If |incognito| is true and we're not in managed mode,
  // clicking on the button will cause the profile menu to be displayed.
  AvatarMenuButton(Browser* browser, bool incognito);

  virtual ~AvatarMenuButton();

  // views::MenuButton:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;

  virtual void SetAvatarIcon(const gfx::Image& icon,
                             bool is_gaia_picture);

  void ShowAvatarBubble();

 private:
  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  Browser* browser_;
  bool incognito_;
  scoped_ptr<ui::MenuModel> menu_model_;

  // Use a scoped ptr because gfx::Image doesn't have a default constructor.
  scoped_ptr<gfx::Image> icon_;
  gfx::ImageSkia button_icon_;
  bool is_gaia_picture_;
  int old_height_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUTTON_H_
