// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_oauth_helper.h"

#include "base/message_loop/message_loop.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

SigninOAuthHelper::SigninOAuthHelper(net::URLRequestContextGetter* getter,
                                     const std::string& session_index,
                                     const std::string& signin_scoped_device_id,
                                     Consumer* consumer)
    : gaia_auth_fetcher_(this, GaiaConstants::kChromeSource, getter),
      consumer_(consumer) {
  DCHECK(consumer_);
  DCHECK(getter);
  DCHECK(!session_index.empty());
  gaia_auth_fetcher_.StartCookieForOAuthLoginTokenExchangeWithDeviceId(
      session_index, signin_scoped_device_id);
}

SigninOAuthHelper::~SigninOAuthHelper() {}

void SigninOAuthHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  refresh_token_ = result.refresh_token;
  gaia_auth_fetcher_.StartOAuthLogin(result.access_token,
                                     GaiaConstants::kGaiaService);
}

void SigninOAuthHelper::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnClientOAuthFailure: " << error.ToString();
  consumer_->OnSigninOAuthInformationFailure(error);
}

void SigninOAuthHelper::OnClientLoginSuccess(const ClientLoginResult& result) {
  gaia_auth_fetcher_.StartGetUserInfo(result.lsid);
}

void SigninOAuthHelper::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnClientLoginFailure: " << error.ToString();
  consumer_->OnSigninOAuthInformationFailure(error);
}

void SigninOAuthHelper::OnGetUserInfoSuccess(const UserInfoMap& data) {
  UserInfoMap::const_iterator email_iter = data.find("email");
  UserInfoMap::const_iterator display_email_iter = data.find("displayEmail");
  if (email_iter == data.end() || display_email_iter == data.end()) {
    VLOG(1) << "SigninOAuthHelper::OnGetUserInfoSuccess: no email found:"
            << " email=" << email_iter->second
            << " displayEmail=" << display_email_iter->second;
    consumer_->OnSigninOAuthInformationFailure(
        GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
  } else {
    VLOG(1) << "SigninOAuthHelper::OnGetUserInfoSuccess:"
            << " email=" << email_iter->second
            << " displayEmail=" << display_email_iter->second;
    consumer_->OnSigninOAuthInformationAvailable(
        email_iter->second, display_email_iter->second, refresh_token_);
  }
}

void SigninOAuthHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "SigninOAuthHelper::OnGetUserInfoFailure : " << error.ToString();
  consumer_->OnSigninOAuthInformationFailure(error);
}
