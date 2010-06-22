// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_

#include <string>

#include "app/menus/simple_menu_model.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class ImageView;
class Menu2;
class MenuButton;
class Throbber;
}  // namespace views

namespace chromeos {

class SignoutView;

class UserView : public views::View,
                 public views::ButtonListener,
                 public views::ViewMenuDelegate,
                 public menus::SimpleMenuModel::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies that user pressed signout button on screen locker.
    virtual void OnSignout() {}

    // Notifies that user would like to remove this user from login screen.
    virtual void OnRemoveUser() {}

    // Notifies that user would like to take new picture for this user on
    // login screen.
    virtual void OnChangePhoto() {}
  };

  // Creates UserView for login screen (|is_login| == true) or screen locker.
  // On login screen this will have addition menu with user specific actions.
  // On screen locker it will have sign out button.
  UserView(Delegate* delegate, bool is_login);

  // view::View overrides.
  virtual gfx::Size GetPreferredSize();

  // Sets the user's image.
  void SetImage(const SkBitmap& image);

  // Sets tooltip over the image.
  void SetTooltipText(const std::wstring& text);

  // Start/Stop throbber.
  void StartThrobber();
  void StopThrobber();

  // Show/Hide menu for user specific actions.
  void SetMenuVisible(bool flag);

  // Enable/Disable sign-out button.
  void SetSignoutEnabled(bool enabled);

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

  // menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  void Init();
  void BuildMenu();

  Delegate* delegate_;

  SignoutView* signout_view_;

  // For editing the password.
  views::ImageView* image_view_;

  views::Throbber* throbber_;

  // Menu for user specific actions.
  scoped_ptr<menus::SimpleMenuModel> menu_model_;
  scoped_ptr<views::Menu2> menu_;
  views::MenuButton* menu_button_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_VIEW_H_

