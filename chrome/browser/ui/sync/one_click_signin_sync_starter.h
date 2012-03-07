// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
#pragma once

#include <string>

#include "chrome/browser/signin/signin_tracker.h"

class Profile;

// Waits for successful singin notification from the signin manager and then
// starts the sync machine.  Instances of this class delete themselves once
// the job is done.
class OneClickSigninSyncStarter : public SigninTracker::Observer {
 public:
  OneClickSigninSyncStarter(const std::string& session_index,
                            const std::string& email,
                            const std::string& password,
                            Profile* profile,
                            bool use_default_settings);

 private:
  virtual ~OneClickSigninSyncStarter();

  // content::SigninTracker::Observer override.
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  Profile* profile_;
  SigninTracker signin_tracker_;
  bool use_default_settings_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarter);
};


#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_SYNC_STARTER_H_
