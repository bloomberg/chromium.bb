// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_mapper_util.h"

#include "components/signin/core/browser/account_tracker_service.h"

namespace chromeos {

AccountMapperUtil::AccountMapperUtil(
    AccountTrackerService* account_tracker_service)
    : account_tracker_service_(account_tracker_service) {}

AccountMapperUtil::~AccountMapperUtil() = default;

std::string AccountMapperUtil::AccountKeyToOAuthAccountId(
    const AccountManager::AccountKey& account_key) const {
  DCHECK(account_key.IsValid());

  if (account_key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return std::string();
  }

  const std::string& account_id =
      account_tracker_service_->FindAccountInfoByGaiaId(account_key.id)
          .account_id;
  DCHECK(!account_id.empty()) << "Can't find account id";
  return account_id;
}

AccountManager::AccountKey AccountMapperUtil::OAuthAccountIdToAccountKey(
    const std::string& account_id) const {
  DCHECK(!account_id.empty());

  const AccountInfo& account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  DCHECK(!account_info.gaia.empty()) << "Can't find account info";
  return AccountManager::AccountKey{
      account_info.gaia, account_manager::AccountType::ACCOUNT_TYPE_GAIA};
}

}  // namespace chromeos
