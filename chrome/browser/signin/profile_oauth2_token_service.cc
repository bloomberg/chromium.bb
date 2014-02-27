// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_oauth2_token_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "net/url_request/url_request_context_getter.h"

ProfileOAuth2TokenService::ProfileOAuth2TokenService()
    : profile_(NULL) {
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  DCHECK(!signin_global_error_.get()) <<
      "ProfileOAuth2TokenService::Initialize called but not "
      "ProfileOAuth2TokenService::Shutdown";
}

void ProfileOAuth2TokenService::Initialize(Profile* profile) {
  DCHECK(profile);
  DCHECK(!profile_);
  profile_ = profile;

  signin_global_error_.reset(new SigninGlobalError(profile));
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      signin_global_error_.get());
}

void ProfileOAuth2TokenService::Shutdown() {
  DCHECK(profile_) << "Shutdown() called without matching call to Initialize()";
  GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
      signin_global_error_.get());
  signin_global_error_.reset();
}

std::string ProfileOAuth2TokenService::GetRefreshToken(
    const std::string& account_id) const {
  NOTREACHED() << "GetRefreshToken should not be called on the base PO2TS";
  return "";
}

net::URLRequestContextGetter* ProfileOAuth2TokenService::GetRequestContext() {
  return NULL;
}

void ProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  NOTREACHED();
}

std::string ProfileOAuth2TokenService::GetPrimaryAccountId() {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile_);
  // TODO(fgorski): DCHECK(signin_manager) here - it may require update to test
  // code and the line above (SigninManager might not exist yet).
  return signin_manager ? signin_manager->GetAuthenticatedUsername()
      : std::string();
}

std::vector<std::string> ProfileOAuth2TokenService::GetAccounts() {
  NOTREACHED();
  return std::vector<std::string>();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  // Empty implementation by default.
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  NOTREACHED();
}

void ProfileOAuth2TokenService::RevokeAllCredentials() {
  // Empty implementation by default.
}

