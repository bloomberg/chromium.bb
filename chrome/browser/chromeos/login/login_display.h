// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/remove_user_delegate.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace chromeos {

// TODO(nkostylev): Extract interface, create a BaseLoginDisplay class.
// An abstract class that defines login UI implementation.
class LoginDisplay : public RemoveUserDelegate {
 public:
  class Delegate {
   public:
    // Create new Google account.
    virtual void CreateAccount() = 0;

    // Returns name of the currently connected network.
    virtual string16 GetConnectedNetworkName() = 0;

    // Users decides to sign in into captive portal.
    virtual void FixCaptivePortal() = 0;

    // Sets the displayed email for the next login attempt with |CompleteLogin|.
    // If it succeeds, user's displayed email value will be updated to |email|.
    virtual void SetDisplayEmail(const std::string& email) = 0;

    // Complete sign process with specified |username| and |password|.
    // Used for new users authenticated through an extension.
    virtual void CompleteLogin(const std::string& username,
                               const std::string& password) = 0;

    // Sign in using |username| and |password| specified.
    // Used for both known users only.
    virtual void Login(const std::string& username,
                       const std::string& password) = 0;

    // Sign in into Guest session.
    virtual void LoginAsGuest() = 0;

    // Called when existing user pod is selected in the UI.
    virtual void OnUserSelected(const std::string& username) = 0;

    // Called when the user requests enterprise enrollment.
    virtual void OnStartEnterpriseEnrollment() = 0;

   protected:
    virtual ~Delegate();
  };

  // |background_bounds| determines the bounds of login UI background.
  LoginDisplay(Delegate* delegate, const gfx::Rect& background_bounds);
  virtual ~LoginDisplay();

  // Initializes login UI with the user pods based on list of known users and
  // guest, new user pods if those are enabled.
  virtual void Init(const UserList& users,
                    bool show_guest,
                    bool show_new_user) = 0;

  // Called when user image has been changed.
  // |user| contains updated user.
  virtual void OnUserImageChanged(const User& user) = 0;

  // After this call login display should be ready to be smoothly destroyed
  // (e.g. hide throbber, etc.).
  virtual void OnFadeOut() = 0;

  // Called when user is successfully authenticated.
  virtual void OnLoginSuccess(const std::string& username) = 0;

  // Changes enabled state of the UI.
  virtual void SetUIEnabled(bool is_enabled) = 0;

  // Selects user entry with specified |index|.
  // Does nothing if current user is already selected.
  virtual void SelectPod(int index) = 0;

  // Displays error with |error_msg_id| specified.
  // |login_attempts| shows number of login attempts made by current user.
  // |help_topic_id| is additional help topic that is presented as link.
  virtual void ShowError(int error_msg_id,
                         int login_attempts,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;

  gfx::Rect background_bounds() const { return background_bounds_; }
  void set_background_bounds(const gfx::Rect background_bounds){
    background_bounds_ = background_bounds;
  }

  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  gfx::NativeWindow parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeWindow window) { parent_window_ = window; }

  int width() const { return background_bounds_.width(); }

 protected:
  // Login UI delegate (controller).
  Delegate* delegate_;

  // Parent window, might be used to create dialog windows.
  gfx::NativeWindow parent_window_;

  // Bounds of the login UI background.
  gfx::Rect background_bounds_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_H_
