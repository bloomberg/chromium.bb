// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MAPPER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MAPPER_UTIL_H_

#include <string>

#include "base/macros.h"
#include "chromeos/account_manager/account_manager.h"

class AccountTrackerService;

namespace chromeos {

class AccountMapperUtil {
 public:
  explicit AccountMapperUtil(AccountTrackerService* account_tracker_service);
  ~AccountMapperUtil();

  // A utility method to map an |account_key| to the account id used by the
  // OAuth2TokenService chain (see |AccountInfo|). Returns an empty string for
  // non-Gaia accounts.
  std::string AccountKeyToOAuthAccountId(
      const AccountManager::AccountKey& account_key) const;

  // A utility method to map the |account_id| used by the OAuth2TokenService
  // chain (see |AccountInfo|) to an |AccountManager::AccountKey|.
  AccountManager::AccountKey OAuthAccountIdToAccountKey(
      const std::string& account_id) const;

 private:
  // A non-owning pointer to |AccountTrackerService|, which itself is a
  // |KeyedService|.
  AccountTrackerService* const account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountMapperUtil);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MAPPER_UTIL_H_
