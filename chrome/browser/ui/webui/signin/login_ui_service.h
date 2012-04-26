// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace content {
class WebUI;
};

// The LoginUIService helps track per-profile information for the login UI -
// for example, whether there is login UI currently on-screen.
class LoginUIService : public ProfileKeyedService {
 public:
  // Creates a LoginUIService associated with the passed |profile|. |profile|
  // is used to create a new browser in the case that ShowLoginUI() is invoked
  // when no browser windows are open (e.g. via the Mac menu bar).
  explicit LoginUIService(Profile* profile);
  virtual ~LoginUIService();

  // Gets the currently active login UI, or null if no login UI is active.
  content::WebUI* current_login_ui() const {
    return ui_;
  }

  // Sets the currently active login UI. It is illegal to call this if there is
  // already login UI visible.
  void SetLoginUI(content::WebUI* ui);

  // Called when login UI is closed. If the passed UI is the current login UI,
  // sets current_login_ui() to null.
  void LoginUIClosed(content::WebUI* ui);

  // Brings the current login UI for this profile to the foreground (it is an
  // error to call this if there is no visible login UI.
  void FocusLoginUI();

  // Displays the login dialog if the user is not yet logged in, otherwise
  // displays the sync setup dialog. If |force_login| is true, then the login
  // UI is displayed even if the user is already logged in (useful if we need
  // to gather GAIA credentials for oauth tokens). Virtual for mocking purposes.
  // TODO(atwilson): Refactor this API to make the behavior more clear and use
  // an enum instead of a boolean (http://crbug.com/118795).
  virtual void ShowLoginUI(bool force_login);

 private:
  // Weak pointer to the currently active login UI, or null if none.
  content::WebUI* ui_;

  // Weak pointer to the profile this service is associated with.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIService);
};


#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
