// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_

#include "base/string16.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"

namespace views {
class NativeButton;
class WidgetGtk;
}

namespace chromeos {

// UserController manages the set of windows needed to login a single existing
// user. ExistingUserController creates the nececessary set of UserControllers.
class UserController : public views::ButtonListener,
                       public views::Textfield::Controller {
 public:
  class Delegate {
   public:
    virtual void Login(UserController* source,
                       const string16& password) = 0;
  };

  // Creates a UserController representing the guest (other user) login.
  UserController();

  // Creates a UserController for the specified user.
  UserController(Delegate* delegate, const UserManager::User& user);

  ~UserController();

  // Initializes the UserController, creating the set of windows/controls.
  // |index| is the index of this user, and |total_user_count| the total
  // number of users.
  void Init(int index, int total_user_count);

  const UserManager::User& user() const { return user_; }

  // Sets the enabled state of the password field to |enable|.
  void SetPasswordEnabled(bool enable);

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Textfield::Controller:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);

  // Max size needed when an entry is selected.
  static const int kSize;

  // Padding between the user windows.
  static const int kPadding;

  // Max size needed when an entry is not selected.
  static const int kUnselectedSize;

 private:
  // Invoked when the user wants to login. Forwards the call to the delegate.
  void Login();

  views::WidgetGtk* CreateControlsWindow(int index, int* height);
  views::WidgetGtk* CreateImageWindow(int index);
  views::WidgetGtk* CreateBorderWindow(int index,
                                       int total_user_count,
                                       int controls_height);
  views::WidgetGtk* CreateLabelWindow(int index, WmIpc::WindowType type);

  // Is this the guest user?
  const bool is_guest_;

  // If is_guest_ is false, this is the user being shown.
  const UserManager::User user_;

  Delegate* delegate_;

  // For editing the password.
  views::Textfield* password_field_;

  // Button to start login.
  views::NativeButton* submit_button_;

  // A window is used to represent the individual chunks.
  views::WidgetGtk* controls_window_;
  views::WidgetGtk* image_window_;
  views::WidgetGtk* border_window_;
  views::WidgetGtk* label_window_;
  views::WidgetGtk* unselected_label_window_;

  DISALLOW_COPY_AND_ASSIGN(UserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_
