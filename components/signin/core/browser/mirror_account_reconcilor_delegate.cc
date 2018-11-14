// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_reconcilor.h"

namespace signin {

MirrorAccountReconcilorDelegate::MirrorAccountReconcilorDelegate(
    identity::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
}

MirrorAccountReconcilorDelegate::~MirrorAccountReconcilorDelegate() {
  identity_manager_->RemoveObserver(this);
}

bool MirrorAccountReconcilorDelegate::IsReconcileEnabled() const {
  return identity_manager_->HasPrimaryAccount();
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

std::vector<std::string>
MirrorAccountReconcilorDelegate::ReorderChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const signin::MultiloginMode mode) const {
  DCHECK(mode ==
         signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
  std::vector<std::string> accounts_to_send;
  accounts_to_send.reserve(chrome_accounts.size());

  DCHECK(!primary_account.empty());
  accounts_to_send.push_back(primary_account);

  for (const std::string& chrome_account : chrome_accounts) {
    if (chrome_account == primary_account)
      continue;
    accounts_to_send.push_back(chrome_account);
  }
  DCHECK(!accounts_to_send.empty());
  return accounts_to_send;
}

void MirrorAccountReconcilorDelegate::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  reconcilor()->EnableReconcile();
}

void MirrorAccountReconcilorDelegate::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  reconcilor()->DisableReconcile(true /* logout_all_gaia_accounts */);
}

}  // namespace signin
