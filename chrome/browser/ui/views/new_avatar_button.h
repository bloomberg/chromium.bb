// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_AVATAR_BUTTON_H_

#include "ui/views/controls/button/menu_button.h"

// Avatar button that displays the active profile's name in the caption area.
class NewAvatarButton : public views::MenuButton {
 public:
  // Different button styles that can be applied.
  enum AvatarButtonStyle {
    THEMED_BUTTON,   // Used in a themed browser window.
    GLASS_BUTTON,    // Used in a native aero window.
    METRO_BUTTON     // Used in a metro window.
  };

  NewAvatarButton(views::ButtonListener* listener,
                  const string16& profile_name,
                  AvatarButtonStyle button_style);
  virtual ~NewAvatarButton();

  // views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  friend class NewAvatarMenuButtonTest;
  FRIEND_TEST_ALL_PREFIXES(NewAvatarMenuButtonTest, SignOut);

  DISALLOW_COPY_AND_ASSIGN(NewAvatarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_AVATAR_BUTTON_H_
