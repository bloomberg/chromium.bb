// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_

#include <string>

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_metrics.h"

// Handles details of signing the user in with SigninManager and turning on
// sync for an account that is already present in the token service.
class DiceTurnSyncOnHelper {
 public:
  DiceTurnSyncOnHelper(Profile* profile,
                       Browser* browser,
                       signin_metrics::AccessPoint signin_access_point,
                       signin_metrics::Reason signin_reason,
                       const std::string& account_id);
  virtual ~DiceTurnSyncOnHelper();

 private:
  // Handles can offer sign-in errors.  It returns true if there is an error,
  // and false otherwise.
  bool HandleCanOfferSigninError();

  // Handles cross account sign in error. If |account_info_| does not match the
  // last authenticated account of the current profile, then Chrome will show a
  // confirmation dialog before starting sync. It returns true if there is a
  // cross account error, and false otherwise.
  bool HandleCrossAccountError();

  // Callback used with ConfirmEmailDialogDelegate.
  void ConfirmEmailAction(content::WebContents* web_contents,
                          SigninEmailConfirmationDialog::Action action);

  // Creates the sync starter.
  void CreateSyncStarter(OneClickSigninSyncStarter::ProfileMode profile_mode);

  Profile* profile_;
  Browser* browser_;
  signin_metrics::AccessPoint signin_access_point_;
  signin_metrics::Reason signin_reason_;
  const AccountInfo account_info_;

  DISALLOW_COPY_AND_ASSIGN(DiceTurnSyncOnHelper);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_H_
