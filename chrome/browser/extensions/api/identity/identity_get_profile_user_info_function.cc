// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"

#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/identity/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {

IdentityGetProfileUserInfoFunction::IdentityGetProfileUserInfoFunction() {
}

IdentityGetProfileUserInfoFunction::~IdentityGetProfileUserInfoFunction() {
}

ExtensionFunction::ResponseAction IdentityGetProfileUserInfoFunction::Run() {
  if (GetProfile()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  if (!extension()->permissions_data()->HasAPIPermission(
          APIPermission::kIdentityEmail)) {
    api::identity::ProfileUserInfo profile_user_info;
    return RespondNow(OneArgument(profile_user_info.ToValue()));
  }

  content::BrowserContext::GetConnectorFor(GetProfile())
      ->BindInterface(identity::mojom::kServiceName,
                      mojo::MakeRequest(&identity_manager_));

  identity_manager_->GetPrimaryAccountId(base::Bind(
      &IdentityGetProfileUserInfoFunction::OnReceivedPrimaryAccountId, this));

  return RespondLater();
}

void IdentityGetProfileUserInfoFunction::OnReceivedPrimaryAccountId(
    const base::Optional<AccountId>& account_id) {
  DCHECK(extension()->permissions_data()->HasAPIPermission(
      APIPermission::kIdentityEmail));

  api::identity::ProfileUserInfo profile_user_info;

  if (account_id) {
    profile_user_info.email = account_id->GetUserEmail();
    profile_user_info.id = account_id->GetGaiaId();
  }

  Respond(OneArgument(profile_user_info.ToValue()));
}

}  // namespace extensions
