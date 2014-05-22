// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_

#include <set>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"

class Profile;

// Shows auth errors on the wrench menu using a bubble view and a menu item.
class SigninGlobalError : public GlobalErrorWithStandardBubble,
                          public SigninErrorController::Observer,
                          public KeyedService {
 public:
  SigninGlobalError(SigninErrorController* error_controller,
                    Profile* profile);
  virtual ~SigninGlobalError();

  // Returns true if there is an authentication error.
  bool HasError();

  // Shows re-authentication UI to the user in an attempt to fix the error.
  // The re-authentication UI will be shown in |browser|.
  void AttemptToFixError(Browser* browser);

 private:
  FRIEND_TEST_ALL_PREFIXES(SigninGlobalErrorTest, NoErrorAuthStatusProviders);
  FRIEND_TEST_ALL_PREFIXES(SigninGlobalErrorTest, ErrorAuthStatusProvider);
  FRIEND_TEST_ALL_PREFIXES(SigninGlobalErrorTest, AuthStatusEnumerateAllErrors);

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // GlobalErrorWithStandardBubble:
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual base::string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual base::string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<base::string16> GetBubbleViewMessages() OVERRIDE;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual base::string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // SigninErrorController::Observer:
  virtual void OnErrorChanged() OVERRIDE;

  // The Profile this service belongs to.
  Profile* profile_;

  // The SigninErrorController that provides auth status.
  SigninErrorController* error_controller_;

  // True if signin global error was added to the global error service.
  bool is_added_to_global_error_service_;

  DISALLOW_COPY_AND_ASSIGN(SigninGlobalError);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
