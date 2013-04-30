// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"

namespace policy {

UserCloudPolicyTokenForwarder::UserCloudPolicyTokenForwarder(
    UserCloudPolicyManagerChromeOS* manager,
    TokenService* token_service)
    : manager_(manager),
      token_service_(token_service) {
  if (token_service_->HasOAuthLoginToken()) {
    manager_->OnRefreshTokenAvailable(
        token_service_->GetOAuth2LoginRefreshToken());
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service_));
  }
}

UserCloudPolicyTokenForwarder::~UserCloudPolicyTokenForwarder() {}

void UserCloudPolicyTokenForwarder::Shutdown() {
  registrar_.RemoveAll();
}

void UserCloudPolicyTokenForwarder::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE);
  if (token_service_->HasOAuthLoginToken()) {
    registrar_.RemoveAll();
    manager_->OnRefreshTokenAvailable(
        token_service_->GetOAuth2LoginRefreshToken());
  }
}

}  // namespace policy
