// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_

#include <set>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/signin_error_controller.h"

class Profile;

// Shows auth errors on the wrench menu using a bubble view and a menu item.
class SigninGlobalError : public GlobalErrorWithStandardBubble,
                          public SigninErrorController::Observer,
                          public KeyedService {
 public:
  SigninGlobalError(SigninErrorController* error_controller,
                    Profile* profile);
  virtual ~SigninGlobalError();

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

 private:
  // The Profile this service belongs to.
  Profile* profile_;

  // The SigninErrorController that provides auth status.
  SigninErrorController* error_controller_;

  DISALLOW_COPY_AND_ASSIGN(SigninGlobalError);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
