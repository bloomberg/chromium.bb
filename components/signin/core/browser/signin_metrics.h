// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_

#include "base/time/time.h"

#include "google_apis/gaia/google_service_auth_error.h"

namespace signin_metrics {

// Enum for the ways in which primary account detection is done.
enum DifferentPrimaryAccounts {
  // token and cookie had same primary accounts.
  ACCOUNTS_SAME = 0,
  // Deprecated. Indicates different primary accounts.
  UNUSED_ACCOUNTS_DIFFERENT,
  // No GAIA cookie present, so the primaries are considered different.
  NO_COOKIE_PRESENT,
  // There was at least one cookie and one token, and the primaries differed.
  COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
  NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS,
};

// Track all the ways a profile can become signed out as a histogram.
enum ProfileSignout {
  // The value used within unit tests
  SIGNOUT_TEST = 0,
  // The preference or policy controlling if signin is valid has changed.
  SIGNOUT_PREF_CHANGED = 0,
  // The valid pattern for signing in to the Google service changed.
  GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
  // The preference or policy controlling if signin is valid changed during
  // the signin process.
  SIGNIN_PREF_CHANGED_DURING_SIGNIN,
  // User clicked to signout from the settings page.
  USER_CLICKED_SIGNOUT_SETTINGS,
  // The signin process was aborted, but signin had succeeded, so signout. This
  // may be due to a server response, policy definition or user action.
  ABORT_SIGNIN,
  // The sync server caused the profile to be signed out.
  SERVER_FORCED_DISABLE,
  // The credentials are being transfered to a new profile, so the old one is
  // signed out.
  TRANSFER_CREDENTIALS,

  // Keep this as the last enum.
  NUM_PROFILE_SIGNOUT_METRICS,
};

// Enum values used for use with "AutoLogin.Reverse" histograms.
enum {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,
  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,
  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,
  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,
  // The user completely ignored the infoar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,
  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,
  // The sync was started with default settings.
  HISTOGRAM_WITH_DEFAULTS,
  // The sync was started with advanced settings.
  HISTOGRAM_WITH_ADVANCED,
  // The sync was started through auto-accept with default settings.
  HISTOGRAM_AUTO_WITH_DEFAULTS,
  // The sync was started through auto-accept with advanced settings.
  HISTOGRAM_AUTO_WITH_ADVANCED,
  // The sync was aborted with an undo button.
  HISTOGRAM_UNDO,
  HISTOGRAM_MAX
};

// Enum values used with the "Signin.OneClickConfirmation" histogram, which
// tracks the actions used in the OneClickConfirmation bubble.
enum {
  HISTOGRAM_CONFIRM_SHOWN,
  HISTOGRAM_CONFIRM_OK,
  HISTOGRAM_CONFIRM_RETURN,
  HISTOGRAM_CONFIRM_ADVANCED,
  HISTOGRAM_CONFIRM_CLOSE,
  HISTOGRAM_CONFIRM_ESCAPE,
  HISTOGRAM_CONFIRM_UNDO,
  HISTOGRAM_CONFIRM_LEARN_MORE,
  HISTOGRAM_CONFIRM_LEARN_MORE_OK,
  HISTOGRAM_CONFIRM_LEARN_MORE_RETURN,
  HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED,
  HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE,
  HISTOGRAM_CONFIRM_LEARN_MORE_ESCAPE,
  HISTOGRAM_CONFIRM_LEARN_MORE_UNDO,
  HISTOGRAM_CONFIRM_MAX
};

// Enum valus used with the "Signin.SigninSource" histogram, which tracks the
// source that launched a Gaia signin page.
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

// Enum values used for use with the "Signin.Reauth" histogram.
enum AccountReauth {
  // The user gave the wrong email when doing a reauthentication.
  HISTOGRAM_ACCOUNT_MISSMATCH,
  // The user was shown a reauthentication login screen.
  HISTOGRAM_REAUTH_SHOWN,

  HISTOGRAM_REAUTH_MAX
};

// Log to UMA histograms and UserCounts stats about a single execution of the
// AccountReconciler.
// |total_number_accounts| - How many accounts are in the browser for this
//                           profile.
// |count_added_to_cookie_jar| - How many accounts were in the browser but not
//                               in the cookie jar.
// |count_removed_from_cookie_jar| - How many accounts were in the cookie jar
//                                   but not in the browser.
// |primary_accounts_same| - False if the primary account for the cookie jar
//                           and the token service were different; else true.
// |is_first_reconcile| - True if these stats are from the first execution of
//                        the AccountReconcilor.
// |pre_count_gaia_cookies| - How many GAIA cookies were present before
//                            the AccountReconcilor began modifying the state.
void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_removed_from_cookie_jar,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies);

// Track a successful signin.
void LogSigninAddAccount();

// Tracks the original source that showed the signin page.
void LogSigninSource(Source source);

// Track a successful signin of a profile.
void LogSigninProfile(bool is_first_run, base::Time install_date);

// Track a profile signout.
void LogSignout(ProfileSignout metric);

// Tracks whether the external connection results were all fetched before
// the reconcilor tried to use them with MergeSession.
// |time_to_check_connections| is the time it took to complete.
void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections);

// Track when the current authentication error changed.
void LogAuthError(GoogleServiceAuthError::State auth_error);

void LogSigninConfirmHistogramValue(int action);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
