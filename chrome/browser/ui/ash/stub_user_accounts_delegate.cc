// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/stub_user_accounts_delegate.h"

#include <cctype>

#include "base/logging.h"

StubUserAccountsDelegate::StubUserAccountsDelegate(const std::string& owner_id)
    : primary_account_(owner_id) {}

StubUserAccountsDelegate::~StubUserAccountsDelegate() {}

std::string StubUserAccountsDelegate::GetPrimaryAccountId() {
  return primary_account_;
}

std::vector<std::string> StubUserAccountsDelegate::GetSecondaryAccountIds() {
  return secondary_accounts_;
}

std::string StubUserAccountsDelegate::GetAccountDisplayName(
    const std::string& account_id) {
  std::string res(1, std::toupper(account_id[0]));
  res += account_id.substr(1);
  return res;
}

void StubUserAccountsDelegate::DeleteAccount(const std::string& account_id) {
  secondary_accounts_.erase(
      std::remove(
          secondary_accounts_.begin(), secondary_accounts_.end(), account_id),
      secondary_accounts_.end());
  NotifyAccountListChanged();
}

void StubUserAccountsDelegate::AddAccount(const std::string& account_id) {
  if (primary_account_ == account_id)
    return;
  if (std::find(secondary_accounts_.begin(),
                secondary_accounts_.end(),
                account_id) != secondary_accounts_.end())
    return;
  secondary_accounts_.push_back(account_id);
  NotifyAccountListChanged();
}

void StubUserAccountsDelegate::LaunchAddAccountDialog() {
  NOTIMPLEMENTED();
}
