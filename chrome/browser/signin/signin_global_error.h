// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_

#include <set>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;

// Shows auth errors on the wrench menu using a bubble view and a
// menu item. Services that wish to expose auth errors to the user should
// register an AuthStatusProvider to report their current authentication state,
// and should invoke AuthStatusChanged() when their authentication state may
// have changed.
class SigninGlobalError : public GlobalError {
 public:
  class AuthStatusProvider {
   public:
    AuthStatusProvider();
    virtual ~AuthStatusProvider();

    // API invoked by SigninGlobalError to get the current auth status of
    // the various signed in services.
    virtual GoogleServiceAuthError GetAuthStatus() const = 0;
  };

  explicit SigninGlobalError(Profile* profile);
  virtual ~SigninGlobalError();

  // Adds a provider which the SigninGlobalError object will start querying for
  // auth status.
  void AddProvider(const AuthStatusProvider* provider);

  // Removes a provider previously added by SigninGlobalError (generally only
  // called in preparation for shutdown).
  void RemoveProvider(const AuthStatusProvider* provider);

  // Invoked when the auth status of an AuthStatusProvider has changed.
  void AuthStatusChanged();

  // GlobalError implementation.
  virtual bool HasBadge() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

 private:
  std::set<const AuthStatusProvider*> provider_set_;

  // The auth error detected the last time AuthStatusChanged() was invoked (or
  // NONE if AuthStatusChanged() has never been invoked).
  GoogleServiceAuthError auth_error_;

  // The Profile this object belongs to.
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
