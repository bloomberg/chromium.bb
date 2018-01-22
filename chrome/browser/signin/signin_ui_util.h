// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/buildflag.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_features.h"
#include "components/signin/core/browser/signin_metrics.h"

class Profile;
class Browser;
class SigninManagerBase;

// Utility functions to gather status information from the various signed in
// services and construct messages suitable for showing in UI.
namespace signin_ui_util {

// The maximum number of times to show the welcome tutorial for an upgrade user.
const int kUpgradeWelcomeTutorialShowMax = 1;

// Returns the username of the authenticated user or an empty string if there is
// no authenticated user.
base::string16 GetAuthenticatedUsername(const SigninManagerBase* signin);

// Initializes signin-related preferences.
void InitializePrefsForProfile(Profile* profile);

// Shows a learn more page for signin errors.
void ShowSigninErrorLearnMorePage(Profile* profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Returns the display email string for the given account.  If the profile
// has not been migrated to use gaia ids, then its possible for the display
// to not ne known yet.  In this case, use |account_id|, which is assumed to
// be an email address.
std::string GetDisplayEmail(Profile* profile, const std::string& account_id);

// Returns the list of all accounts that have a token. The default account in
// the Gaia cookies will be the first account in the list.
std::vector<AccountInfo> GetAccountsForDicePromos(Profile* profile);

// Enables sync for |account| if the refresh token is valid or presents the
// Chrome sign-in page with |account.email| prefilled if the token is missing
// or invalid.
//
// Note: This function does nothing if the user is already signed in to Chrome.
void EnableSync(Browser* browser,
                const AccountInfo& account,
                signin_metrics::AccessPoint access_point);
#endif

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
