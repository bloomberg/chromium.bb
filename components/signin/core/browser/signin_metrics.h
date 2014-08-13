// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_

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

// Log to UMA histograms and UserCounts stats about a single execution of the
// AccountReconciler.
// |total_number_accounts| - How many accounts are in the browser for this
//                           profile.
// |count_added_to_cookie_jar| - How many accounts were in the browser but not
//                               the cookie jar.
// |count_added_to_token| - How may accounts were in the cookie jar but not in
//                          the browser.
// |primary_accounts_same| - False if the primary account for the cookie jar
//                           and the token service were different; else true.
// |is_first_reconcile| - True if these stats are from the first execution of
//                        the AccountReconcilor.
// |pre_count_gaia_cookies| - How many GAIA cookies were present before
//                            the AccountReconcilor began modifying the state.
void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_added_to_token,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies);

// Track a successful signin.
void LogSigninAddAccount();

// Track a profile signout.
void LogSignout(ProfileSignout metric);

// Tracks whether the external connection results were all fetched before
// the reconcilor tried to use them with MergeSession.
void LogExternalCcResultFetches(bool fetches_completed);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_H_
