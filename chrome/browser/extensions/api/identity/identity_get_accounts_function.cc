// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"

#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"
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

  std::vector<std::string> gaia_ids =
      IdentityAPI::GetFactoryInstance()->Get(GetProfile())->GetAccounts();
  DCHECK(gaia_ids.size() < 2 || switches::IsExtensionsMultiAccount());

  std::unique_ptr<base::ListValue> infos(new base::ListValue());

  for (std::vector<std::string>::const_iterator it = gaia_ids.begin();
       it != gaia_ids.end();
       ++it) {
    api::identity::AccountInfo account_info;
    account_info.id = *it;
    infos->Append(account_info.ToValue());
  }

  return RespondNow(OneArgument(std::move(infos)));
}

}  // namespace extensions
