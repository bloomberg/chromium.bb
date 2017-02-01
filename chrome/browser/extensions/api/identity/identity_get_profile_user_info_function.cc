// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"

#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

IdentityGetProfileUserInfoFunction::IdentityGetProfileUserInfoFunction() {
}

IdentityGetProfileUserInfoFunction::~IdentityGetProfileUserInfoFunction() {
}

ExtensionFunction::ResponseAction IdentityGetProfileUserInfoFunction::Run() {
  if (GetProfile()->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  AccountInfo account = SigninManagerFactory::GetForProfile(
      GetProfile())->GetAuthenticatedAccountInfo();
  api::identity::ProfileUserInfo profile_user_info;
  if (extension()->permissions_data()->HasAPIPermission(
          APIPermission::kIdentityEmail)) {
    profile_user_info.email = account.email;
    profile_user_info.id = account.gaia;
  }

  return RespondNow(OneArgument(profile_user_info.ToValue()));
}

}  // namespace extensions
