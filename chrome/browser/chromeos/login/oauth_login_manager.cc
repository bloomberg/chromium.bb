// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oauth_login_manager.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_switches.h"

using content::BrowserThread;

namespace chromeos {

// OAuthLoginManager.

// static.
OAuthLoginManager* OAuthLoginManager::Create(
    OAuthLoginManager::Delegate* delegate) {
  return new OAuth2LoginManager(delegate);
}

void OAuthLoginManager::CompleteAuthentication() {
  delegate_->OnCompletedAuthentication(user_profile_);
  TokenService* token_service =
      TokenServiceFactory::GetForProfile(user_profile_);
  if (token_service->AreCredentialsValid())
    token_service->StartFetchingTokens();
}

OAuthLoginManager::OAuthLoginManager(Delegate* delegate)
    : delegate_(delegate),
      user_profile_(NULL),
      restore_strategy_(RESTORE_FROM_COOKIE_JAR),
      state_(SESSION_RESTORE_NOT_STARTED) {
}

OAuthLoginManager::~OAuthLoginManager() {
}

}  // namespace chromeos
