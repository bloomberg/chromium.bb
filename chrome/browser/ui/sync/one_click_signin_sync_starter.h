// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_

#include <string>

#include "chrome/browser/signin/signin_tracker.h"

class Browser;

// Waits for successful singin notification from the signin manager and then
// starts the sync machine.  Instances of this class delete themselves once
// the job is done.
class OneClickSigninSyncStarter : public SigninTracker::Observer {
 public:
  enum StartSyncMode {
    // Starts the process of signing the user in with the SigninManager, and
    // once completed automatically starts sync with all data types enabled.
    SYNC_WITH_DEFAULT_SETTINGS,

    // Starts the process of signing the user in with the SigninManager, and
    // once completed redirects the user to the settings page to allow them
    // to configure which data types to sync before sync is enabled.
    CONFIGURE_SYNC_FIRST
  };

  OneClickSigninSyncStarter(Browser* browser,
                            const std::string& session_index,
                            const std::string& email,
                            const std::string& password,
                            StartSyncMode start_mode);

 private:
  virtual ~OneClickSigninSyncStarter();

  // SigninTracker::Observer override.
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  Browser* browser_;
  SigninTracker signin_tracker_;
  StartSyncMode start_mode_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarter);
};


#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
