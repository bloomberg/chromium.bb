// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"

namespace extensions {

IdentityGetAccountsFunction::IdentityGetAccountsFunction() {
}

IdentityGetAccountsFunction::~IdentityGetAccountsFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAccountsFunction::Run() {
  if (GetProfile()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile());
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(GetProfile());

  std::unique_ptr<base::ListValue> infos(new base::ListValue());
  std::string primary_account =
      SigninManagerFactory::GetForProfile(GetProfile())
          ->GetAuthenticatedAccountId();

  // If there is no primary account, short-circuit out.
  if (primary_account.empty() ||
      !token_service->RefreshTokenIsAvailable(primary_account))
    return RespondNow(OneArgument(std::move(infos)));

  // Set the primary account as the first entry.
  std::string primary_gaia_id =
      account_tracker->GetAccountInfo(primary_account).gaia;
  api::identity::AccountInfo primary_account_info;
  primary_account_info.id = primary_gaia_id;
  infos->Append(primary_account_info.ToValue());

  // If extensions are not multi-account, ignore any other accounts.
  if (!switches::IsExtensionsMultiAccount())
    return RespondNow(OneArgument(std::move(infos)));

  // Otherwise, add the other accounts.
  std::vector<std::string> accounts = token_service->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    std::string gaia_id = account_tracker->GetAccountInfo(*it).gaia;
    if (gaia_id.empty())
      continue;
    if (gaia_id == primary_gaia_id)
      continue;

    api::identity::AccountInfo account_info;
    account_info.id = gaia_id;
    infos->Append(account_info.ToValue());
  }

  return RespondNow(OneArgument(std::move(infos)));
}

}  // namespace extensions
