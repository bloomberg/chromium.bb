// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_identity_strategy.h"

#include "chrome/browser/browser_signin.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/guid.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace policy {

namespace em = enterprise_management;

UserPolicyIdentityStrategy::UserPolicyIdentityStrategy(
    Profile* profile,
    const FilePath& cache_file)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  cache_ = new UserPolicyTokenCache(weak_ptr_factory_.GetWeakPtr(), cache_file);
  registrar_.Add(this,
                 NotificationType::TOKEN_AVAILABLE,
                 Source<TokenService>(profile->GetTokenService()));

  // Register for the event of user login. The device management token won't
  // be fetched until we know the domain of the currently logged in user.
#if defined(OS_CHROMEOS)
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_CHANGED,
                 NotificationService::AllSources());
#else
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
                 Source<Profile>(profile_));
#endif
}

UserPolicyIdentityStrategy::~UserPolicyIdentityStrategy() {}

void UserPolicyIdentityStrategy::LoadTokenCache() {
  cache_->Load();
}

std::string UserPolicyIdentityStrategy::GetDeviceToken() {
  return device_token_;
}

std::string UserPolicyIdentityStrategy::GetDeviceID() {
  return device_id_;
}

std::string UserPolicyIdentityStrategy::GetMachineID() {
  return std::string();
}

std::string UserPolicyIdentityStrategy::GetMachineModel() {
  return std::string();
}

em::DeviceRegisterRequest_Type
UserPolicyIdentityStrategy::GetPolicyRegisterType() {
  return em::DeviceRegisterRequest::USER;
}

std::string UserPolicyIdentityStrategy::GetPolicyType() {
  return kChromeUserPolicyType;
}

bool UserPolicyIdentityStrategy::GetCredentials(std::string* username,
                                                std::string* auth_token) {
  *username = GetCurrentUser();
  *auth_token = profile_->GetTokenService()->GetTokenForService(
      GaiaConstants::kDeviceManagementService);

  return !username->empty() && !auth_token->empty() && !device_id_.empty();
}

void UserPolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  DCHECK(!device_id_.empty());
  device_token_ = token;
  cache_->Store(device_token_, device_id_);
  NotifyDeviceTokenChanged();
}

std::string UserPolicyIdentityStrategy::GetCurrentUser() {
#if defined(OS_CHROMEOS)
  // TODO(mnissler) On CrOS it seems impossible to figure out what user belongs
  // to a profile. Revisit after multi-profile support landed.
  return chromeos::UserManager::Get()->logged_in_user().email();
#else
  return profile_->GetBrowserSignin()->GetSignedInUsername();
#endif
}

void UserPolicyIdentityStrategy::CheckAndTriggerFetch() {
  if (!GetCurrentUser().empty() &&
      profile_->GetTokenService()->HasTokenForService(
          GaiaConstants::kDeviceManagementService)) {
    // For user tokens, there is no actual identifier. We generate a random
    // identifier instead each time we ask for the token.
    device_id_ = guid::GenerateGUID();
    NotifyAuthChanged();
  }
}

void UserPolicyIdentityStrategy::OnTokenCacheLoaded(
    const std::string& token,
    const std::string& device_id) {
  if (!token.empty() && !device_id.empty()) {
    device_token_ = token;
    device_id_ = device_id;
    NotifyDeviceTokenChanged();
  } else {
    CheckAndTriggerFetch();
  }
}

void UserPolicyIdentityStrategy::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == NotificationType::TOKEN_AVAILABLE) {
    if (Source<TokenService>(source).ptr() == profile_->GetTokenService()) {
      const TokenService::TokenAvailableDetails* token_details =
          Details<const TokenService::TokenAvailableDetails>(details).ptr();
      if (token_details->service() == GaiaConstants::kDeviceManagementService)
        if (device_token_.empty()) {
          // Request a new device management server token, but only in case we
          // don't already have it.
          CheckAndTriggerFetch();
        }
    }
#if defined(OS_CHROMEOS)
  } else if (type == NotificationType::LOGIN_USER_CHANGED) {
    CheckAndTriggerFetch();
#else
  } else if (type == NotificationType::GOOGLE_SIGNIN_SUCCESSFUL) {
    if (profile_ == Source<Profile>(source).ptr())
      CheckAndTriggerFetch();
#endif
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
