// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_oauth_helper.h"

#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

// TODO(guohui): needs to figure out the UI for error cases.

SigninOAuthHelper::SigninOAuthHelper(Profile* profile)
    : profile_(profile) {}

SigninOAuthHelper::~SigninOAuthHelper() {}

void SigninOAuthHelper::StartAddingAccount(const std::string& oauth_code) {
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, profile_->GetRequestContext()));
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(oauth_code);
}

void SigninOAuthHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  DVLOG(1) << "SigninOAuthHelper::OnClientOAuthSuccess";

  refresh_token_ = result.refresh_token;
  gaia_auth_fetcher_->StartOAuthLogin(result.access_token,
                                      GaiaConstants::kGaiaService);
}

void SigninOAuthHelper::OnClientOAuthFailure(
      const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnClientOAuthFailure : " << error.ToString();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninOAuthHelper::OnClientLoginSuccess(const ClientLoginResult& result) {
  DVLOG(1) << "SigninOAuthHelper::OnClientLoginSuccess";
  gaia_auth_fetcher_->StartGetUserInfo(result.lsid);
}

void SigninOAuthHelper::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnClientLoginFailure : " << error.ToString();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninOAuthHelper::OnGetUserInfoSuccess(const UserInfoMap& data) {
  DVLOG(1) << "SigninOAuthHelper::OnGetUserInfoSuccess";

  UserInfoMap::const_iterator email_iter = data.find("email");
  if (email_iter == data.end()) {
    VLOG(1) << "SigninOAuthHelper::OnGetUserInfoSuccess : no email found ";
  } else {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
        ->UpdateCredentials(email_iter->second, refresh_token_);
  }
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SigninOAuthHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnGetUserInfoFailure : " << error.ToString();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
