// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"

namespace signin_metrics {

void LogSigninAccountReconciliation(int total_number_accounts,
                                    int count_added_to_cookie_jar,
                                    int count_added_to_token,
                                    bool primary_accounts_same,
                                    bool is_first_reconcile) {
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfAccountsPerProfile",
                           total_number_accounts);
  // We want to include zeroes in the counts of added accounts to easily
  // capture _relatively_ how often we merge accounts.
  if (is_first_reconcile) {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.FirstRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToChrome.FirstRun",
                             count_added_to_token);
    UMA_HISTOGRAM_BOOLEAN("Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
                          !primary_accounts_same);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToCookieJar.SubsequentRun",
                             count_added_to_cookie_jar);
    UMA_HISTOGRAM_COUNTS_100("Signin.Reconciler.AddedToChrome.SubsequentRun",
                             count_added_to_token);
    UMA_HISTOGRAM_BOOLEAN(
        "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
        !primary_accounts_same);
  }
}

void LogSigninAddAccount() {
  // Account signin may fail for a wide variety of reasons. There is no
  // explicit false, but one can compare this value with the various UI
  // flows that lead to account sign-in, and deduce that the difference
  // counts the failures.
  UMA_HISTOGRAM_BOOLEAN("Signin.AddAccount", true);
}

}  // namespace signin_metrics
