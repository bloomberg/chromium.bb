// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_refresh_token_fetcher.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using GaiaConstants::kChromeSyncSupervisedOAuth2Scope;
using base::Time;
using gaia::GaiaOAuthClient;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;

namespace {

const int kNumRetries = 1;

static const char kIssueTokenBodyFormat[] =
    "client_id=%s"
    "&scope=%s"
    "&response_type=code"
    "&profile_id=%s"
    "&device_name=%s";

// kIssueTokenBodyFormatDeviceIdAddendum is appended to kIssueTokenBodyFormat
// if device_id is provided.
// TODO(pavely): lib_ver is passed to differentiate IssueToken requests from
// different code locations. Remove once device_id mismatch is understood.
// (crbug.com/481596)
static const char kIssueTokenBodyFormatDeviceIdAddendum[] =
    "&device_id=%s&lib_ver=supervised_user";

static const char kAuthorizationHeaderFormat[] =
    "Authorization: Bearer %s";

static const char kCodeKey[] = "code";

class SupervisedUserRefreshTokenFetcherImpl
    : public SupervisedUserRefreshTokenFetcher,
      public OAuth2TokenService::Consumer,
      public URLFetcherDelegate,
      public GaiaOAuthClient::Delegate {
 public:
  SupervisedUserRefreshTokenFetcherImpl(
      OAuth2TokenService* oauth2_token_service,
      const std::string& account_id,
      const std::string& device_id,
      URLRequestContextGetter* context);
  ~SupervisedUserRefreshTokenFetcherImpl() override;

  // SupervisedUserRefreshTokenFetcher implementation:
  void Start(const std::string& supervised_user_id,
             const std::string& device_name,
             const TokenCallback& callback) override;

 protected:
  // OAuth2TokenService::Consumer implementation:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const URLFetcher* source) override;

  // GaiaOAuthClient::Delegate implementation:
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  // Requests an access token, which is the first thing we need. This is where
  // we restart when the returned access token has expired.
  void StartFetching();

  void DispatchNetworkError(int error_code);
  void DispatchGoogleServiceAuthError(const GoogleServiceAuthError& error,
                                      const std::string& token);
  OAuth2TokenService* oauth2_token_service_;
  std::string account_id_;
  std::string device_id_;
  URLRequestContextGetter* context_;

  std::string device_name_;
  std::string supervised_user_id_;
  TokenCallback callback_;

  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;
  std::string access_token_;
  bool access_token_expired_;
  std::unique_ptr<URLFetcher> url_fetcher_;
  std::unique_ptr<GaiaOAuthClient> gaia_oauth_client_;
};

SupervisedUserRefreshTokenFetcherImpl::SupervisedUserRefreshTokenFetcherImpl(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    const std::string& device_id,
    URLRequestContextGetter* context)
    : OAuth2TokenService::Consumer("supervised_user"),
      oauth2_token_service_(oauth2_token_service),
      account_id_(account_id),
      device_id_(device_id),
      context_(context),
      access_token_expired_(false) {}

SupervisedUserRefreshTokenFetcherImpl::
~SupervisedUserRefreshTokenFetcherImpl() {}

void SupervisedUserRefreshTokenFetcherImpl::Start(
    const std::string& supervised_user_id,
    const std::string& device_name,
    const TokenCallback& callback) {
  DCHECK(callback_.is_null());
  supervised_user_id_ = supervised_user_id;
  device_name_ = device_name;
  callback_ = callback;
  StartFetching();
}

void SupervisedUserRefreshTokenFetcherImpl::StartFetching() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  access_token_request_ = oauth2_token_service_->StartRequest(
      account_id_, scopes, this);
}

void SupervisedUserRefreshTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const Time& expiration_time) {
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_ = access_token;

  GURL url(GaiaUrls::GetInstance()->oauth2_issue_token_url());
  // GaiaOAuthClient uses id 0, so we use 1 to distinguish the requests in
  // unit tests.
  const int id = 1;

  url_fetcher_ = URLFetcher::Create(id, url, URLFetcher::POST, this);

  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(),
      data_use_measurement::DataUseUserData::SUPERVISED_USER);
  url_fetcher_->SetRequestContext(context_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  url_fetcher_->AddExtraRequestHeader(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));

  std::string body = base::StringPrintf(
      kIssueTokenBodyFormat,
      net::EscapeUrlEncodedData(
          GaiaUrls::GetInstance()->oauth2_chrome_client_id(), true).c_str(),
      net::EscapeUrlEncodedData(kChromeSyncSupervisedOAuth2Scope, true).c_str(),
      net::EscapeUrlEncodedData(supervised_user_id_, true).c_str(),
      net::EscapeUrlEncodedData(device_name_, true).c_str());
  if (!device_id_.empty()) {
    body.append(base::StringPrintf(
        kIssueTokenBodyFormatDeviceIdAddendum,
        net::EscapeUrlEncodedData(device_id_, true).c_str()));
  }
  url_fetcher_->SetUploadData("application/x-www-form-urlencoded", body);

  url_fetcher_->Start();
}

void SupervisedUserRefreshTokenFetcherImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_.get(), request);
  callback_.Run(error, std::string());
  callback_.Reset();
}

void SupervisedUserRefreshTokenFetcherImpl::OnURLFetchComplete(
    const URLFetcher* source) {
  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DispatchNetworkError(status.error());
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_UNAUTHORIZED && !access_token_expired_) {
    access_token_expired_ = true;
    oauth2_token_service_->InvalidateAccessToken(
        account_id_, OAuth2TokenService::ScopeSet(), access_token_);
    StartFetching();
    return;
  }

  if (response_code != net::HTTP_OK) {
    // TODO(bauerb): We should return the HTTP response code somehow.
    DLOG(WARNING) << "HTTP error " << response_code;
    DispatchGoogleServiceAuthError(
        GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
        std::string());
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response_body);
  base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict)) {
    DispatchNetworkError(net::ERR_INVALID_RESPONSE);
    return;
  }
  std::string auth_code;
  if (!dict->GetString(kCodeKey, &auth_code)) {
    DispatchNetworkError(net::ERR_INVALID_RESPONSE);
    return;
  }

  gaia::OAuthClientInfo client_info;
  GaiaUrls* urls = GaiaUrls::GetInstance();
  client_info.client_id = urls->oauth2_chrome_client_id();
  client_info.client_secret = urls->oauth2_chrome_client_secret();
  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(context_));
  gaia_oauth_client_->GetTokensFromAuthCode(client_info, auth_code, kNumRetries,
                                            this);
}

void SupervisedUserRefreshTokenFetcherImpl::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  // TODO(bauerb): It would be nice if we could pass the access token as well,
  // so we don't need to fetch another one immediately.
  DispatchGoogleServiceAuthError(GoogleServiceAuthError::AuthErrorNone(),
                                 refresh_token);
}

void SupervisedUserRefreshTokenFetcherImpl::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  NOTREACHED();
}

void SupervisedUserRefreshTokenFetcherImpl::OnOAuthError() {
  DispatchGoogleServiceAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
      std::string());
}

void SupervisedUserRefreshTokenFetcherImpl::OnNetworkError(int response_code) {
  // TODO(bauerb): We should return the HTTP response code somehow.
  DLOG(WARNING) << "HTTP error " << response_code;
  DispatchGoogleServiceAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
      std::string());
}

void SupervisedUserRefreshTokenFetcherImpl::DispatchNetworkError(
    int error_code) {
  DispatchGoogleServiceAuthError(
      GoogleServiceAuthError::FromConnectionError(error_code), std::string());
}

void SupervisedUserRefreshTokenFetcherImpl::DispatchGoogleServiceAuthError(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  callback_.Run(error, token);
  callback_.Reset();
}

}  // namespace

// static
std::unique_ptr<SupervisedUserRefreshTokenFetcher>
SupervisedUserRefreshTokenFetcher::Create(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    const std::string& device_id,
    URLRequestContextGetter* context) {
  std::unique_ptr<SupervisedUserRefreshTokenFetcher> fetcher(
      new SupervisedUserRefreshTokenFetcherImpl(
          oauth2_token_service, account_id, device_id, context));
  return fetcher;
}

SupervisedUserRefreshTokenFetcher::~SupervisedUserRefreshTokenFetcher() {}
