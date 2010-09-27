// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/chromeos/login/new_user_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget_delegate.h"

namespace views {
class NativeButton;
class WidgetGtk;
}

namespace chromeos {

class UserView;
class ExistingUserView;

// UserController manages the set of windows needed to login a single existing
// user or first time login for a new user. ExistingUserController creates
// the nececessary set of UserControllers.
class UserController : public views::ButtonListener,
                       public views::Textfield::Controller,
                       public views::WidgetDelegate,
                       public NewUserView::Delegate,
                       public NotificationObserver,
                       public UserView::Delegate {
 public:
  class Delegate {
   public:
    virtual void Login(UserController* source,
                       const string16& password) = 0;
    virtual void LoginOffTheRecord() = 0;
    virtual void ClearErrors() = 0;
    virtual void OnUserSelected(UserController* source) = 0;
    virtual void ActivateWizard(const std::string& screen_name) = 0;
    virtual void RemoveUser(UserController* source) = 0;
    virtual void AddStartUrl(const GURL& start_url) = 0;

    // Selects user entry with specified |index|.
    // Does nothing if current user is already selected.
    virtual void SelectUser(int index) = 0;
   protected:
    virtual ~Delegate() {}
  };

  // Creates a UserController representing new user or bwsi login.
  UserController(Delegate* delegate, bool is_bwsi);

  // Creates a UserController for the specified user.
  UserController(Delegate* delegate, const UserManager::User& user);

  ~UserController();

  // Initializes the UserController, creating the set of windows/controls.
  // |index| is the index of this user, and |total_user_count| the total
  // number of users.
  void Init(int index, int total_user_count, bool need_browse_without_signin);

  // Update border window parameters to notify window manager about new numbers.
  // |index| of this user and |total_user_count| of users.
  void UpdateUserCount(int index, int total_user_count);

  int user_index() const { return user_index_; }
  bool is_user_selected() const { return is_user_selected_; }
  bool is_new_user() const { return is_new_user_; }
  bool is_bwsi() const { return is_bwsi_; }
  NewUserView* new_user_view() const { return new_user_view_; }

  const UserManager::User& user() const { return user_; }

  // Enables or disables tooltip with user's email.
  void EnableNameTooltip(bool enable);

  // Resets password text and sets the enabled state of the password.
  void ClearAndEnablePassword();

  // Called when user view is activated (OnUserSelected).
  void ClearAndEnableFields();

  // Returns bounds of password field in screen coordinates.
  // For new user it returns username coordinates.
  gfx::Rect GetScreenBounds() const;

  // Get widget that contains all controls.
  views::WidgetGtk* controls_window() {
    return controls_window_;
  }

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Textfield::Controller:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);

  // views::WidgetDelegate:
  virtual void IsActiveChanged(bool active);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // NewUserView::Delegate
  virtual void OnLogin(const std::string& username,
                       const std::string& password);
  virtual void OnCreateAccount();
  virtual void OnLoginOffTheRecord();
  virtual void AddStartUrl(const GURL& start_url) {
    delegate_->AddStartUrl(start_url);
  }
  virtual void ClearErrors();
  virtual void NavigateAway();

  // UserView::Delegate implementation:
  virtual void OnRemoveUser();

  // Selects user entry with specified |index|, |is_click| is true if the entry
  // was selected by mouse click.
  void SelectUser(int index, bool is_click);

  // Sets focus on password field.
  void FocusPasswordField();

  // Padding between the user windows.
  static const int kPadding;

  // Max size needed when an entry is not selected.
  static const int kUnselectedSize;

 private:
  // Invoked when the user wants to login. Forwards the call to the delegate.
  void Login();

  // Performs common setup for login windows.
  void ConfigureLoginWindow(views::WidgetGtk* window,
                            int index,
                            const gfx::Rect& bounds,
                            chromeos::WmIpcWindowType type,
                            views::View* contents_view);
  views::WidgetGtk* CreateControlsWindow(int index,
                                         int* height,
                                         bool need_browse_without_signin);
  views::WidgetGtk* CreateImageWindow(int index);
  views::WidgetGtk* CreateLabelWindow(int index, WmIpcWindowType type);
  void CreateBorderWindow(int index, int total_user_count, int controls_height);

  // Sets specified image on the image window. If image's size is less than
  // 75% of window size, image size is preserved to avoid blur. Otherwise,
  // the image is resized to fit window size precisely. Image view repaints
  // itself.
  void SetImage(const SkBitmap& image);

  // Sets the enabled state of the password field to |enable|.
  void SetPasswordEnabled(bool enable);

  // Returns tooltip text for user name.
  std::wstring GetNameTooltip() const;

  // User index within all the users.
  int user_index_;

  // Is this user selected now?
  bool is_user_selected_;

  // Is this the new user pod?
  const bool is_new_user_;

  // Is this the bwsi pod?
  const bool is_bwsi_;

  // Should we show tooltips above user image and label to help distinguish
  // users with the same display name.
  bool show_name_tooltip_;

  // If is_new_user_ and is_bwsi_ are false, this is the user being shown.
  UserManager::User user_;

  Delegate* delegate_;

  // A window is used to represent the individual chunks.
  views::WidgetGtk* controls_window_;
  views::WidgetGtk* image_window_;
  views::WidgetGtk* border_window_;
  views::WidgetGtk* label_window_;
  views::WidgetGtk* unselected_label_window_;

  // View that shows user image on image window.
  UserView* user_view_;

  // View that is used for new user login.
  NewUserView* new_user_view_;

  // View that is used for existing user login.
  ExistingUserView* existing_user_view_;

  // Views that show display name of the user.
  views::Label* label_view_;
  views::Label* unselected_label_view_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_CONTROLLER_H_
