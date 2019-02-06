// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/browser_context.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace extensions {

IdentityGetAccountsFunction::IdentityGetAccountsFunction() {
}

IdentityGetAccountsFunction::~IdentityGetAccountsFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAccountsFunction::Run() {
  if (browser_context()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  std::vector<AccountInfo> accounts =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()))
          ->GetAccountsWithRefreshTokens();
  std::unique_ptr<base::ListValue> infos(new base::ListValue());

  if (accounts.empty()) {
    return RespondNow(OneArgument(std::move(infos)));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  bool primary_account_only = IdentityAPI::GetFactoryInstance()
                                  ->Get(profile)
                                  ->AreExtensionsRestrictedToPrimaryAccount();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  api::identity::AccountInfo account_info;
  if (primary_account_only) {
    // If extensions are restricted to the primary account, only return the
    // account info when there is a valid primary account.
    if (identity_manager->HasPrimaryAccountWithRefreshToken()) {
      account_info.id = identity_manager->GetPrimaryAccountInfo().gaia;
      infos->Append(account_info.ToValue());
    }
  } else {
    for (const auto& account : accounts) {
      account_info.id = account.gaia;
      infos->Append(account_info.ToValue());
    }
  }

  return RespondNow(OneArgument(std::move(infos)));
}

}  // namespace extensions
