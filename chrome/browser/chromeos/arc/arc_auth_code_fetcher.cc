// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_code_fetcher.h"

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_auth_code_fetcher_delegate.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

namespace arc {

namespace {

constexpr int kGetAuthCodeNetworkRetry = 3;

constexpr char kConsumerName[] = "ArcAuthContext";
constexpr char kToken[] = "token";
constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceType[] = "device_type";
constexpr char kDeviceTypeArc[] = "arc_plus_plus";
constexpr char kLoginScopedToken[] = "login_scoped_token";
constexpr char kGetAuthCodeHeaders[] =
    "Content-Type: application/json; charset=utf-8";
constexpr char kContentTypeJSON[] = "application/json";

}  // namespace

ArcAuthCodeFetcher::ArcAuthCodeFetcher(
    ArcAuthCodeFetcherDelegate* delegate,
    net::URLRequestContextGetter* request_context_getter,
    Profile* profile,
    const std::string& auth_endpoint)
    : OAuth2TokenService::Consumer(kConsumerName),
      delegate_(delegate),
      request_context_getter_(request_context_getter),
      profile_(profile),
      auth_endpoint_(auth_endpoint) {
  // Get token service and account ID to fetch auth tokens.
  ProfileOAuth2TokenService* const token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  const SigninManagerBase* const signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  CHECK(token_service && signin_manager);
  const std::string& account_id = signin_manager->GetAuthenticatedAccountId();
  DCHECK(!account_id.empty());
  DCHECK(token_service->RefreshTokenIsAvailable(account_id));

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_.reset(
      token_service->StartRequest(account_id, scopes, this).release());
}

ArcAuthCodeFetcher::~ArcAuthCodeFetcher() {}

void ArcAuthCodeFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  ResetFetchers();

  const std::string device_id = user_manager::known_user::GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(profile_));
  DCHECK(!device_id.empty());

  base::DictionaryValue request_data;
  request_data.SetString(kLoginScopedToken, access_token);
  request_data.SetString(kDeviceType, kDeviceTypeArc);
  request_data.SetString(kDeviceId, device_id);
  std::string request_string;
  base::JSONWriter::Write(request_data, &request_string);

  DCHECK(!auth_endpoint_.empty());
  auth_code_fetcher_ = net::URLFetcher::Create(0, GURL(auth_endpoint_),
                                               net::URLFetcher::POST, this);
  auth_code_fetcher_->SetRequestContext(request_context_getter_);
  auth_code_fetcher_->SetUploadData(kContentTypeJSON, request_string);
  auth_code_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                                   net::LOAD_BYPASS_CACHE);
  auth_code_fetcher_->SetAutomaticallyRetryOnNetworkChanges(
      kGetAuthCodeNetworkRetry);
  auth_code_fetcher_->SetExtraRequestHeaders(kGetAuthCodeHeaders);
  auth_code_fetcher_->Start();
}

void ArcAuthCodeFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  VLOG(2) << "Failed to get LST " << error.ToString() << ".";
  ResetFetchers();

  delegate_->OnAuthCodeFailed();
}

void ArcAuthCodeFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  const int response_code = source->GetResponseCode();
  std::string json_string;
  source->GetResponseAsString(&json_string);

  ResetFetchers();

  if (response_code != net::HTTP_OK) {
    VLOG(2) << "Server returned wrong response code: " << response_code << ".";
    delegate_->OnAuthCodeFailed();
    return;
  }

  JSONStringValueDeserializer deserializer(json_string);
  std::string error_msg;
  std::unique_ptr<base::Value> auth_code_info =
      deserializer.Deserialize(nullptr, &error_msg);
  if (!auth_code_info) {
    VLOG(2) << "Unable to deserialize auth code json data: " << error_msg
            << ".";
    delegate_->OnAuthCodeFailed();
    return;
  }

  std::unique_ptr<base::DictionaryValue> auth_code_dictionary =
      base::DictionaryValue::From(std::move(auth_code_info));
  if (!auth_code_dictionary) {
    NOTREACHED();
    delegate_->OnAuthCodeFailed();
    return;
  }

  std::string auth_code;
  if (!auth_code_dictionary->GetString(kToken, &auth_code) ||
      auth_code.empty()) {
    VLOG(2) << "Response does not contain auth code.";
    delegate_->OnAuthCodeFailed();
    return;
  }

  delegate_->OnAuthCodeSuccess(auth_code);
}

void ArcAuthCodeFetcher::ResetFetchers() {
  login_token_request_.reset();
  auth_code_fetcher_.reset();
}

}  // namespace arc
