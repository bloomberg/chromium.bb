// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"

namespace signin_metrics {

// Helper method to determine which |DifferentPrimaryAccounts| applies.
DifferentPrimaryAccounts ComparePrimaryAccounts(bool primary_accounts_same,
                                                int pre_count_gaia_cookies) {
  if (primary_accounts_same)
    return ACCOUNTS_SAME;
  if (pre_count_gaia_cookies == 0)
    return NO_COOKIE_PRESENT;
  return COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT;
}

void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_added_to_token,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile,
                                    int pre_count_gaia_cookies) {
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfAccountsPerProfile",
                           total_number_accounts);
  // We want to include zeroes in the counts of added accounts to easily
  // capture _relatively_ how often we merge accounts.
  if (is_first_reconcile) {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.FirstRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToChrome.FirstRun",
                             count_added_to_token);
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        ComparePrimaryAccounts(primary_accounts_same, pre_count_gaia_cookies),
        NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.SubsequentRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToChrome.SubsequentRun",
                             count_added_to_token);
    UMA_HISTOGRAM_ENUMERATION(
        "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
        ComparePrimaryAccounts(primary_accounts_same, pre_count_gaia_cookies),
        NUM_DIFFERENT_PRIMARY_ACCOUNT_METRICS);
  }
}

void LogSigninProfile(bool is_first_run, base::Time install_date) {
  // Track whether or not the user signed in during the first run of Chrome.
  UMA_HISTOGRAM_BOOLEAN("Signin.DuringFirstRun", is_first_run);

  // Determine how much time passed since install when this profile was signed
  // in.
  base::TimeDelta elapsed_time = base::Time::Now() - install_date;
  UMA_HISTOGRAM_COUNTS("Signin.ElapsedTimeFromInstallToSignin",
                       elapsed_time.InMinutes());
}

void LogSigninAddAccount() {
  // Account signin may fail for a wide variety of reasons. There is no
  // explicit false, but one can compare this value with the various UI
  // flows that lead to account sign-in, and deduce that the difference
  // counts the failures.
  UMA_HISTOGRAM_BOOLEAN("Signin.AddAccount", true);
}

void LogSignout(ProfileSignout metric) {
  UMA_HISTOGRAM_ENUMERATION("Signin.SignoutProfile", metric,
                            NUM_PROFILE_SIGNOUT_METRICS);
}

void LogExternalCcResultFetches(bool fetches_completed) {
  UMA_HISTOGRAM_BOOLEAN("Signin.Reconciler.AllExternalCcResultCompleted",
                        fetches_completed);
}

}  // namespace signin_metrics
