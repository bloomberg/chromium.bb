// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_reconcilor.h"

namespace signin {

MirrorAccountReconcilorDelegate::MirrorAccountReconcilorDelegate(
    SigninManagerBase* signin_manager)
    : signin_manager_(signin_manager) {
  DCHECK(signin_manager_);
  signin_manager_->AddObserver(this);
}

MirrorAccountReconcilorDelegate::~MirrorAccountReconcilorDelegate() {
  signin_manager_->RemoveObserver(this);
}

bool MirrorAccountReconcilorDelegate::IsReconcileEnabled() const {
  return signin_manager_->IsAuthenticated();
}

bool MirrorAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return true;
}

std::string MirrorAccountReconcilorDelegate::GetGaiaApiSource() const {
  return "ChromiumAccountReconcilor";
}

bool MirrorAccountReconcilorDelegate::ShouldAbortReconcileIfPrimaryHasError()
    const {
  return true;
}

std::string MirrorAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution,
    bool will_logout) const {
  // Mirror only uses the primary account, and it is never empty.
  DCHECK(!primary_account.empty());
  DCHECK(base::ContainsValue(chrome_accounts, primary_account));
  return primary_account;
}

bool MirrorAccountReconcilorDelegate::ReorderChromeAccountsForReconcileIfNeeded(
    const std::vector<std::string>& chrome_accounts,
    const std::string primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    std::vector<std::string>* accounts_to_send) const {
  DCHECK(accounts_to_send);
  accounts_to_send->clear();
  accounts_to_send->reserve(chrome_accounts.size());
  bool needs_reordering = false;

  DCHECK(!primary_account.empty());
  accounts_to_send->push_back(primary_account);

  for (const std::string& chrome_account : chrome_accounts) {
    if (chrome_account == primary_account)
      continue;
    accounts_to_send->push_back(chrome_account);
  }

  if (gaia_accounts.size() != accounts_to_send->size()) {
    needs_reordering = true;
  } else {
    for (size_t i = 0; i < accounts_to_send->size(); ++i) {
      const gaia::ListedAccount& gaia_account = gaia_accounts[i];
      if (gaia_account.id != accounts_to_send->at(i) || !gaia_account.valid) {
        needs_reordering = true;
        break;
      }
    }
  }
  return needs_reordering;
}

void MirrorAccountReconcilorDelegate::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username) {
  reconcilor()->EnableReconcile();
}

void MirrorAccountReconcilorDelegate::GoogleSignedOut(
    const std::string& account_id,
    const std::string& username) {
  reconcilor()->DisableReconcile(true /* logout_all_gaia_accounts */);
}

}  // namespace signin
