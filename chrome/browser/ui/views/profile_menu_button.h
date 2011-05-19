// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/models/simple_menu_model.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"

class Profile;
class ProfileMenuModel;

namespace gfx {
class Canvas;
}

namespace ui {
class Accelerator;
}

namespace views {
class Menu2;
}

// ProfileMenuButton
//
// Shows the button for the multiprofile menu with an image layered
// underneath that displays the profile tag.

class ProfileMenuButton : public views::MenuButton,
                          public views::ViewMenuDelegate,
                          public ui::SimpleMenuModel::Delegate {
 public:
  // DefaultActiveTextShadow is a darkened blue color that works with Windows
  // default theme background coloring.
  static const SkColor kDefaultActiveTextShadow = 0xFF708DB3;
  // InactiveTextShadow is a light gray for inactive default themed buttons.
  static const SkColor kDefaultInactiveTextShadow = SK_ColorLTGRAY;
  // DarkTextShadow is used to shadow names on themed browser frames.
  static const SkColor kDarkTextShadow = SK_ColorGRAY;
  // Space between window controls and end of profile tag.
  static const int kProfileTagHorizontalSpacing = 5;

  ProfileMenuButton(const std::wstring& text, Profile* profile);

  virtual ~ProfileMenuButton();

  // Override MenuButton to clamp text at kMaxTextWidth.
  virtual void SetText(const std::wstring& text) OVERRIDE;

  // ui::SimpleMenuModel::Delegate implementation
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  scoped_ptr<views::Menu2> menu_;
  scoped_ptr<ProfileMenuModel> profile_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_BUTTON_H_

