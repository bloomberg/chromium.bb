// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_service.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/webdata/token_web_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_CHROMEOS)
#include "base/metrics/histogram.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

TokenService::TokenService() : profile_(NULL) {
}

TokenService::~TokenService() {
}

void TokenService::Shutdown() {
}

void TokenService::Initialize(const char* const source,
                              Profile* profile) {
  if (Initialized())
    return;

  DCHECK(!profile_);
  profile_ = profile;
}

void TokenService::AddAuthTokenManually(const std::string& service,
                                        const std::string& auth_token) {
  scoped_refptr<TokenWebData> token_web_data =
      TokenWebData::FromBrowserContext(profile_);
  if (token_web_data.get())
    token_web_data->SetTokenForService(service, auth_token);
}

void TokenService::ResetCredentialsInMemory() {
  // Nothing left to reset.
}

void TokenService::UpdateCredentials(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // No longer use ClientLogin creds.
}

void TokenService::UpdateCredentialsWithOAuth2(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  service->UpdateCredentials(service->GetPrimaryAccountId(),
                             oauth2_tokens.refresh_token);
}

void TokenService::LoadTokensFromDB() {
  // Profile pointer can be null in some tests.
  if (profile_) {
    ProfileOAuth2TokenService* service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    service->LoadCredentials();
  }
}

void TokenService::EraseTokensFromDB() {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  service->RevokeAllCredentials();
}

bool TokenService::TokensLoadedFromDB() const {
  return true;
}

bool TokenService::AreCredentialsValid() const {
  return HasOAuthLoginToken();
}

void TokenService::StartFetchingTokens() {
  // There are no tokens to fetch.
}

// Services dependent on a token will check if a token is available.
// If it isn't, they'll go to sleep until they get a token event.
bool TokenService::HasTokenForService(const char* service) const {
  return false;
}

const std::string& TokenService::GetTokenForService(
    const char* const service) const {
  return EmptyString();
}

bool TokenService::HasOAuthLoginToken() const {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  return service->RefreshTokenIsAvailable(service->GetPrimaryAccountId());
}

void TokenService::IssueAuthTokenForTest(const std::string& service,
                                         const std::string& auth_token) {
  // Only issue oauth2 tokens.
  if (profile_ && service == GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
#if defined(ENABLE_MANAGED_USERS)
    std::string account_id = profile_->IsManaged() ?
        managed_users::kManagedUserPseudoEmail :
        token_service->GetPrimaryAccountId();
#else
    std::string account_id = token_service->GetPrimaryAccountId();
#endif
    DCHECK(!account_id.empty());
    token_service->UpdateCredentials(account_id, auth_token);
  }
}
