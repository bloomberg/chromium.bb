// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/child_account_info_fetcher.h"

#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

const char kFetcherId[] = "child_account_info_fetcher";

// Exponential backoff policy on service flag fetching failure.
const net::BackoffEntry::Policy kBackoffPolicy = {
  0,  // Number of initial errors to ignore without backoff.
  2000,  // Initial delay for backoff in ms.
  2,  // Factor to multiply waiting time by.
  0.2,  // Fuzzing percentage. 20% will spread requests randomly between
        // 80-100% of the calculated time.
  1000 * 60 * 60* 4,  // Maximum time to delay requests by (4 hours).
  -1,  // Don't discard entry even if unused.
  false,  // Don't use the initial delay unless the last request was an error.
};

}  // namespace

ChildAccountInfoFetcher::ChildAccountInfoFetcher(
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context_getter,
    AccountFetcherService* service,
    const std::string& account_id)
    : OAuth2TokenService::Consumer(kFetcherId),
      token_service_(token_service),
      request_context_getter_(request_context_getter),
      service_(service),
      account_id_(account_id),
      backoff_(&kBackoffPolicy) {
  TRACE_EVENT_ASYNC_BEGIN1("AccountFetcherService", kFetcherId, this,
                           "account_id", account_id);
  Fetch();
}

void ChildAccountInfoFetcher::Fetch() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_.reset(
      token_service_->StartRequest(account_id_, scopes, this).release());
}

ChildAccountInfoFetcher::~ChildAccountInfoFetcher() {
  TRACE_EVENT_ASYNC_END0("AccountFetcherService", kFetcherId, this);
}

void ChildAccountInfoFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  TRACE_EVENT_ASYNC_STEP_PAST0("AccountFetcherService", kFetcherId, this,
                               "OnGetTokenSuccess");
  DCHECK_EQ(request, login_token_request_.get());

  gaia_auth_fetcher_.reset(service_->signin_client_->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, request_context_getter_));
  gaia_auth_fetcher_->StartOAuthLogin(access_token,
                                      GaiaConstants::kGaiaService);
}

void ChildAccountInfoFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcher::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  gaia_auth_fetcher_->StartGetUserInfo(result.lsid);
}

void ChildAccountInfoFetcher::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcher::OnGetUserInfoSuccess(const UserInfoMap& data) {
  UserInfoMap::const_iterator services_iter = data.find("allServices");
  if (services_iter != data.end()) {
    std::vector<std::string> service_flags;
    base::SplitString(services_iter->second, ',', &service_flags);
    bool is_child_account =
        std::find(service_flags.begin(), service_flags.end(),
                  AccountTrackerService::kChildAccountServiceFlag) !=
        service_flags.end();
    service_->SetIsChildAccount(account_id_, is_child_account);
  } else {
    DLOG(ERROR) << "ChildAccountInfoFetcher::OnGetUserInfoSuccess: "
                << "GetUserInfo response didn't include allServices field.";
  }
}

void ChildAccountInfoFetcher::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcher::HandleFailure() {
  backoff_.InformOfRequest(false);
  timer_.Start(FROM_HERE, backoff_.GetTimeUntilRelease(), this,
               &ChildAccountInfoFetcher::Fetch);
}
