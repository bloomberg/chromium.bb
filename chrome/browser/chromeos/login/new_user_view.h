// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NEW_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NEW_USER_VIEW_H_
#pragma once

#include <string>

#include "base/task.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/user_input.h"
#include "views/accelerator.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"

namespace views {
class Label;
class MenuButton;
class NativeButton;
}  // namespace views

namespace chromeos {

// View that is used for new user login. It asks for username and password,
// allows to specify language preferences or initiate new account creation.
class NewUserView : public ThrobberHostView,
                    public UserInput,
                    public views::TextfieldController,
                    public views::LinkController,
                    public views::ButtonListener {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // User provided |username|, |password| and initiated login.
    virtual void OnLogin(const std::string& username,
                         const std::string& password) = 0;

    // Initiates incognito login.
    virtual void OnLoginAsGuest() = 0;

    // User initiated new account creation.
    virtual void OnCreateAccount() = 0;

    // User started typing so clear all error messages.
    virtual void ClearErrors() = 0;

    // User tries to navigate away from NewUserView pod.
    virtual void NavigateAway() = 0;
  };

  // If |need_border| is true, RoundedRect border and background are required.
  NewUserView(Delegate* delegate,
              bool need_border,
              bool need_guest_link);

  virtual ~NewUserView();

  // Initialize view layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // Returns bounds of password field in screen coordinates.
  gfx::Rect GetPasswordBounds() const;

  // Returns bounds of username field in screen coordinates.
  gfx::Rect GetUsernameBounds() const;

  // views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void RequestFocus();

  // Setters for textfields.
  void SetUsername(const std::string& username);
  void SetPassword(const std::string& password);

  // Attempt to login with the current field values.
  void Login();

  // views::TextfieldController:
  // Not thread-safe, by virtue of using SetupSession().
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke);
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // ThrobberHostView:
  virtual gfx::Rect CalculateThrobberBounds(views::Throbber* throbber);

  // UserInput:
  virtual void EnableInputControls(bool enabled);
  virtual void ClearAndFocusControls();
  virtual void ClearAndFocusPassword();
  virtual gfx::Rect GetMainInputScreenBounds() const;

  // Navigates "away" to other user pods if allowed.
  // Returns true if event has been processed.
  bool NavigateAway();

 protected:
  // views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View *parent,
                                    views::View *child);
  virtual void OnLocaleChanged();
  void AddChildView(View* view);

 private:
  // Creates Link control and adds it as a child.
  void InitLink(views::Link** link);

  // Delete and recreate native controls that fail to update preferred size
  // after text/locale update.
  void RecreatePeculiarControls();

  // Enable or disable the |sign_in_button_| based on the contents of the
  // |username_field_| and |password_field_|. If there is text in both the
  // button is enabled, otherwise it's disabled.
  void UpdateSignInButtonState();

  // Create view with specified solid background and add it as  child.
  views::View* CreateSplitter(SkColor color);

  // Screen controls.
  // NOTE: sign_in_button_ and languages_menubutton_ are handled with
  // special care: they are recreated on any text/locale change
  // because they are not resized properly.
  views::Textfield* username_field_;
  views::Textfield* password_field_;
  views::Label* title_label_;
  views::Label* title_hint_label_;
  views::View* splitter_up1_;
  views::View* splitter_up2_;
  views::View* splitter_down1_;
  views::View* splitter_down2_;
  views::NativeButton* sign_in_button_;
  views::Link* create_account_link_;
  views::Link* guest_link_;
  views::MenuButton* languages_menubutton_;

  views::Accelerator accel_focus_pass_;
  views::Accelerator accel_focus_user_;
  views::Accelerator accel_login_off_the_record_;
  views::Accelerator accel_toggle_accessibility_;

  // Notifications receiver.
  Delegate* delegate_;

  ScopedRunnableMethodFactory<NewUserView> focus_grabber_factory_;

  LanguageSwitchMenu language_switch_menu_;

  // True when login is in process.
  bool login_in_process_;

  // If true, this view needs RoundedRect border and background.
  bool need_border_;

  // Whether Guest Mode link is needed.
  bool need_guest_link_;

  // Whether create account link is needed. Set to false for now but we may
  // need it back in near future.
  bool need_create_account_;

  // Ordinal position of controls inside view layout.
  int languages_menubutton_order_;
  int sign_in_button_order_;

  DISALLOW_COPY_AND_ASSIGN(NewUserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NEW_USER_VIEW_H_
