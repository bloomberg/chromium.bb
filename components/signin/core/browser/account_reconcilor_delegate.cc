// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor_delegate.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

bool AccountReconcilorDelegate::IsReconcileEnabled() const {
  return false;
}

bool AccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return false;
}

gaia::GaiaSource AccountReconcilorDelegate::GetGaiaApiSource() const {
  NOTREACHED() << "Reconcile is not enabled, no Gaia API calls should be made.";
  return gaia::GaiaSource::kChrome;
}

bool AccountReconcilorDelegate::ShouldAbortReconcileIfPrimaryHasError() const {
  return false;
}

std::string AccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution,
    bool will_logout) const {
  return std::string();
}

MultiloginParameters
AccountReconcilorDelegate::CalculateParametersForMultilogin(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    bool first_execution,
    bool primary_has_error) const {
  const gaia::MultiloginMode mode = CalculateModeForReconcile(
      gaia_accounts, primary_account, first_execution, primary_has_error);
  const std::vector<std::string> accounts_to_send =
      GetChromeAccountsForReconcile(chrome_accounts, primary_account,
                                    gaia_accounts, mode);
  return {mode, accounts_to_send};
}

gaia::MultiloginMode AccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string primary_account,
    bool first_execution,
    bool primary_has_error) const {
  return gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
}

std::vector<std::string>
AccountReconcilorDelegate::ReorderChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& first_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts) const {
  // Gaia only supports kMaxGaiaAccounts. Multilogin and MergeSession calls
  // which go above this count will fail.
  const int kMaxGaiaAccounts = 10;
  DCHECK(!first_account.empty());
  DCHECK(base::ContainsValue(chrome_accounts, first_account));

  // Ordered list of accounts, this is the result of this function.
  std::vector<std::string> ordered_accounts;
  ordered_accounts.reserve(gaia_accounts.size());
  // Set of accounts that must be added to ordered_accounts.
  std::set<std::string> chrome_accounts_set(chrome_accounts.begin(),
                                            chrome_accounts.end());

  // Start from the gaia accounts.
  for (const gaia::ListedAccount& account : gaia_accounts)
    ordered_accounts.push_back(account.id);

  // Keep only accounts that are in chrome_accounts_set.
  for (std::string& account : ordered_accounts) {
    if (chrome_accounts_set.find(account) == chrome_accounts_set.end())
      account = std::string();
    else
      chrome_accounts_set.erase(account);
  }

  // At this point, ordered_accounts only contains accounts that must be kept,
  // and chrome_accounts_set only contains account that are not yet in
  // ordered_accounts.

  // Put first_account in first position if needed, using swap to avoid changing
  // the order of existing cookies.
  auto first_account_it = std::find(ordered_accounts.begin(),
                                    ordered_accounts.end(), first_account);
  if (first_account_it == ordered_accounts.end()) {
    // The first account was not already in the cookies, add it in the first
    // empty spot, or at the end if there is no available spot.
    first_account_it = std::find(ordered_accounts.begin(),
                                 ordered_accounts.end(), std::string());
    if (first_account_it == ordered_accounts.end()) {
      first_account_it =
          ordered_accounts.insert(first_account_it, first_account);
    } else {
      *first_account_it = first_account;
    }
    chrome_accounts_set.erase(first_account);
  }
  std::iter_swap(ordered_accounts.begin(), first_account_it);

  // Add the remaining chrome accounts.
  // First in empty spots.
  auto remaining_accounts_it = chrome_accounts_set.begin();
  for (std::string& account : ordered_accounts) {
    if (remaining_accounts_it == chrome_accounts_set.end())
      break;
    if (account.empty()) {
      account = *remaining_accounts_it;
      ++remaining_accounts_it;
    }
  }
  // Then at the end.
  while (remaining_accounts_it != chrome_accounts_set.end()) {
    ordered_accounts.push_back(*remaining_accounts_it);
    ++remaining_accounts_it;
  }

  // There may still be empty spots left. Compact the vector by moving accounts
  // from the end of the vector into the empty spots.
  auto compacting_it = ordered_accounts.begin();
  while (compacting_it != ordered_accounts.end()) {
    // Remove all the empty accounts at the end.
    while (ordered_accounts.back().empty())
      ordered_accounts.pop_back();
    // Find next empty slot.
    compacting_it =
        std::find(compacting_it, ordered_accounts.end(), std::string());
    // Swap it with the last element.
    if (compacting_it != ordered_accounts.end())
      std::swap(*compacting_it, ordered_accounts.back());
  }

  // All accounts have been added, and ordered_accounts now has the same
  // accounts as chrome_accounts, but in a different order.
  DCHECK_EQ(ordered_accounts.size(), chrome_accounts.size());

  // Keep only kMaxGaiaAccounts.
  if (ordered_accounts.size() > kMaxGaiaAccounts)
    ordered_accounts.resize(kMaxGaiaAccounts);

  return ordered_accounts;
}

std::vector<std::string>
AccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const gaia::MultiloginMode mode) const {
  return std::vector<std::string>();
}

AccountReconcilorDelegate::RevokeTokenOption
AccountReconcilorDelegate::ShouldRevokeSecondaryTokensBeforeReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  return RevokeTokenOption::kDoNotRevoke;
}

bool AccountReconcilorDelegate::ShouldRevokeTokensOnCookieDeleted() {
  return false;
}

base::TimeDelta AccountReconcilorDelegate::GetReconcileTimeout() const {
  return base::TimeDelta::Max();
}

void AccountReconcilorDelegate::OnReconcileError(
    const GoogleServiceAuthError& error) {}

}  // namespace signin
