// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_

#include <string>

#include "base/basictypes.h"

class GURL;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Utility functions for sign in promos.
namespace signin {

const char kSignInPromoQueryKeyAutoClose[] = "auto_close";
const char kSignInPromoQueryKeyContinue[] = "continue";
const char kSignInPromoQueryKeySource[] = "source";
const char kSignInPromoQueryKeyConstrained[] = "constrained";
const char kSignInPromoQueryKeyShowAccountManagement[] =
    "showAccountManagement";

enum Source {
  SOURCE_START_PAGE = 0, // This must be first.
  SOURCE_NTP_LINK,
  SOURCE_MENU,
  SOURCE_SETTINGS,
  SOURCE_EXTENSION_INSTALL_BUBBLE,
  SOURCE_APP_LAUNCHER,
  SOURCE_APPS_PAGE_LINK,
  SOURCE_BOOKMARK_BUBBLE,
  SOURCE_AVATAR_BUBBLE_SIGN_IN,
  SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT,
  SOURCE_DEVICES_PAGE,
  SOURCE_REAUTH,
  SOURCE_UNKNOWN, // This must be last.
};

// Enum values used for Android signin promo actions.
enum AndroidSigninPromoAction {
  // The promo was enabled.
  HISTOGRAM_ANDROID_SIGNIN_PROMO_ENABLED = 0,

  // The promo was shown.
  HISTOGRAM_ANDROID_SIGNIN_PROMO_SHOWN,

  // The promo was declined.
  HISTOGRAM_ANDROID_SIGNIN_PROMO_DECLINED,

  // The promo was accepted.
  HISTOGRAM_ANDROID_SIGNIN_PROMO_ACCEPTED,

  // The promo was accepted and user clicked the settings link.
  HISTOGRAM_ANDROID_SIGNIN_PROMO_ACCEPTED_WITH_ADVANCED,

  HISTOGRAM_ANDROID_SIGNIN_PROMO_MAX,
};

// Returns true if the sign in promo should be visible.
// |profile| is the profile of the tab the promo would be shown on.
bool ShouldShowPromo(Profile* profile);

// Returns true if we should show the sign in promo at startup.
bool ShouldShowPromoAtStartup(Profile* profile, bool is_new_profile);

// Called when the sign in promo has been shown so that we can keep track
// of the number of times we've displayed it.
void DidShowPromoAtStartup(Profile* profile);

// Registers the fact that the user has skipped the sign in promo.
void SetUserSkippedPromo(Profile* profile);

// Gets the sign in landing page URL.
GURL GetLandingURL(const char* option, int value);

// Returns the sign in promo URL wth the given arguments in the query.
// |source| identifies from where the sign in promo is being called, and is
// used to record sync promo UMA stats in the context of the source.
// |auto_close| whether to close the sign in promo automatically when done.
// |is_constrained} whether to load the URL in a constrained window, false
// by default.
GURL GetPromoURL(Source source, bool auto_close);
GURL GetPromoURL(Source source, bool auto_close, bool is_constrained);

// Returns a sign in promo URL specifically for reauthenticating |account_id|.
GURL GetReauthURL(Profile* profile, const std::string& account_id);

// Gets the next page URL from the query portion of the sign in promo URL.
GURL GetNextPageURLForPromoURL(const GURL& url);

// Gets the source from the query portion of the sign in promo URL.
// The source identifies from where the sign in promo was opened.
Source GetSourceForPromoURL(const GURL& url);

// Returns true if the auto_close parameter in the given URL is set to true.
bool IsAutoCloseEnabledInURL(const GURL& url);

// Returns true if the showAccountManagement parameter in the given url is set
// to true.
bool ShouldShowAccountManagement(const GURL& url);

// Returns true if the given URL is the standard continue URL used with the
// sync promo when the web-based flow is enabled.  The query parameters
// of the URL are ignored for this comparison.
bool IsContinueUrlForWebBasedSigninFlow(const GURL& url);

// Forces UseWebBasedSigninFlow() to return true when set; used in tests only.
void ForceWebBasedSigninFlowForTesting(bool force);

// Registers the preferences the Sign In Promo needs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
