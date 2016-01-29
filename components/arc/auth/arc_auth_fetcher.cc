// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/auth/arc_auth_fetcher.h"

#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

namespace arc {

namespace {

const char kGMSCoreClientId[] =
    "1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.googleusercontent.com";

}  // namespace

ArcAuthFetcher::ArcAuthFetcher(net::URLRequestContextGetter* getter,
                               Delegate* delegate)
    : delegate_(delegate), auth_fetcher_(this, std::string(), getter) {
  FetchAuthCode();
}

// static
GURL ArcAuthFetcher::CreateURL() {
  std::string query_string =
      base::StringPrintf("?scope=%s&client_id=%s",
                         GaiaConstants::kOAuth1LoginScope, kGMSCoreClientId);
  return GaiaUrls::GetInstance()->client_login_to_oauth2_url().Resolve(
      query_string);
}

void ArcAuthFetcher::FetchAuthCode() {
  DCHECK(!auth_fetcher_.HasPendingFetch());
  auth_fetcher_.StartCookieForOAuthLoginTokenExchange(
      false, /* fetch_token_from_auth_code */
      std::string(),    /* session_index */
      kGMSCoreClientId, std::string() /* device_id */);
}

void ArcAuthFetcher::OnClientOAuthCode(const std::string& auth_code) {
  DCHECK(!auth_fetcher_.HasPendingFetch());
  delegate_->OnAuthCodeFetched(auth_code);
}

void ArcAuthFetcher::OnClientOAuthFailure(const GoogleServiceAuthError& error) {
  // UNEXPECTED_SERVICE_RESPONSE indicates no cookies in response, but request
  // is completed successfully.
  if (error.state() == GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE) {
    delegate_->OnAuthCodeNeedUI();
  } else {
    VLOG(2) << "ARC Auth request failed: " << error.ToString() << ".";
    delegate_->OnAuthCodeFailed();
  }
}

}  // namespace arc
