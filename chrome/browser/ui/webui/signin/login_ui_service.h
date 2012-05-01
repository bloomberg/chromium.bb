// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

// The LoginUIService helps track per-profile information for the login UI -
// for example, whether there is login UI currently on-screen.
class LoginUIService : public ProfileKeyedService {
 public:
  // Various UI components implement this API to allow LoginUIService to
  // manipulate their associated login UI.
  class LoginUI {
   public:
    // Invoked when the login UI should be brought to the foreground.
    virtual void FocusUI() = 0;

    // Invoked when the login UI should be closed. This can be invoked if the
    // user takes an action that should display new login UI.
    virtual void CloseUI() = 0;
   protected:
    virtual ~LoginUI() {}
  };

  // Creates a LoginUIService associated with the passed |profile|. |profile|
  // is used to create a new browser in the case that ShowLoginUI() is invoked
  // when no browser windows are open (e.g. via the Mac menu bar).
  explicit LoginUIService(Profile* profile);
  virtual ~LoginUIService();

  // Gets the currently active login UI, or null if no login UI is active.
  LoginUI* current_login_ui() const {
    return ui_;
  }

  // Sets the currently active login UI. It is illegal to call this if there is
  // already login UI visible.
  void SetLoginUI(LoginUI* ui);

  // Called when login UI is closed. If the passed UI is the current login UI,
  // sets current_login_ui() to null.
  void LoginUIClosed(LoginUI* ui);

  // Brings the login UI to the foreground, or if there is no login UI,
  // navigates to the login UI page in a new browser tab.
  // Virtual for mocking purposes.
  virtual void ShowLoginUI();

 private:
  // Weak pointer to the currently active login UI, or null if none.
  LoginUI* ui_;

  // Weak pointer to the profile this service is associated with.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIService);
};


#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
