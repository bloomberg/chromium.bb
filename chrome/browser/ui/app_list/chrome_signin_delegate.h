// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_SIGNIN_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/signin_tracker.h"
#include "ui/app_list/signin_delegate.h"

class Profile;

class ChromeSigninDelegate : public app_list::SigninDelegate,
                             public SigninTracker::Observer {
 public:
  explicit ChromeSigninDelegate(Profile* profile);

 private:
  virtual ~ChromeSigninDelegate();

  bool IsActiveSignin();
  void FinishSignin();

  // Overridden from app_list::SigninDelegate:
  virtual bool NeedSignin() OVERRIDE;
  virtual void ShowSignin() OVERRIDE;
  virtual void OpenLearnMore() OVERRIDE;
  virtual void OpenSettings() OVERRIDE;
  virtual string16 GetSigninHeading() OVERRIDE;
  virtual string16 GetSigninText() OVERRIDE;
  virtual string16 GetSigninButtonText() OVERRIDE;
  virtual string16 GetLearnMoreLinkText() OVERRIDE;
  virtual string16 GetSettingsLinkText() OVERRIDE;

  // Overridden from SigninTracker::Observer:
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  Profile* profile_;

  scoped_ptr<SigninTracker> signin_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_SIGNIN_DELEGATE_H_
