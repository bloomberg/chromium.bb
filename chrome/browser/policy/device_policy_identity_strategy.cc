// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_identity_strategy.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace policy {

DevicePolicyIdentityStrategy::DevicePolicyIdentityStrategy()
    : should_register_(false) {
  registrar_.Add(this,
                 NotificationType::TOKEN_AVAILABLE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::OWNERSHIP_TAKEN,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
                 NotificationService::AllSources());

  // TODO(mnissler): Figure out how to read the machine id.
  machine_id_ = "dummy-cros-machine-ID";
}

std::string DevicePolicyIdentityStrategy::GetDeviceToken() {
  return device_token_;
}

std::string DevicePolicyIdentityStrategy::GetDeviceID() {
  return machine_id_;
}

bool DevicePolicyIdentityStrategy::GetCredentials(std::string* username,
                                                  std::string* auth_token) {
  // Only register if requested.
  if (!should_register_)
    return false;

  // Need to know the machine id.
  if (machine_id_.empty())
    return false;

  // Only fetch credentials (and, subsequently, token/policy) when the owner
  // is logged in.
  if (!chromeos::OwnershipService::GetSharedInstance()->CurrentUserIsOwner())
    return false;

  // We need to know about the profile of the logged in user.
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (!profile) {
    NOTREACHED() << "Current user profile inaccessible";
    return false;
  }

  *username = chromeos::UserManager::Get()->logged_in_user().email();
  *auth_token = profile->GetTokenService()->GetTokenForService(
      GaiaConstants::kDeviceManagementService);

  return !username->empty() && !auth_token->empty();
}

void DevicePolicyIdentityStrategy::OnDeviceTokenAvailable(
    const std::string& token) {
  DCHECK(!machine_id_.empty());

  // Reset registration flag, so we only attempt registration once.
  should_register_ = false;

  device_token_ = token;
  NotifyDeviceTokenChanged();
}

void DevicePolicyIdentityStrategy::CheckAndTriggerFetch() {
  std::string username;
  std::string auth_token;
  if (GetCredentials(&username, &auth_token))
    NotifyAuthChanged();
}

void DevicePolicyIdentityStrategy::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  if (type == NotificationType::TOKEN_AVAILABLE) {
    const TokenService::TokenAvailableDetails* token_details =
        Details<const TokenService::TokenAvailableDetails>(details).ptr();
    if (token_details->service() == GaiaConstants::kDeviceManagementService)
      CheckAndTriggerFetch();
  } else if (type == NotificationType::LOGIN_USER_CHANGED) {
    should_register_ = false;
    CheckAndTriggerFetch();
  } else if (type == NotificationType::OWNERSHIP_TAKEN) {
    should_register_ = true;
    CheckAndTriggerFetch();
  } else if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    CheckAndTriggerFetch();
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
