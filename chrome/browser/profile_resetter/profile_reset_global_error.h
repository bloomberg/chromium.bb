// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/profile_resetter/profile_reset_callback.h"
#include "chrome/browser/ui/global_error/global_error.h"

// Shows preferences reset errors on the wrench menu and exposes a menu item to
// launch a bubble view.
class ProfileResetGlobalError : public GlobalError {
 public:
  ProfileResetGlobalError();
  virtual ~ProfileResetGlobalError();

  // GlobalError overrides.
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<string16> GetBubbleViewMessages() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // Sets a callback to be given to the bubble to be called when the user
  // chooses to reset the settings.
  void set_reset_callback(const ProfileResetCallback& reset_callback) {
    reset_callback_ = reset_callback;
  }

 private:
  ProfileResetCallback reset_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetGlobalError);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_GLOBAL_ERROR_H_
