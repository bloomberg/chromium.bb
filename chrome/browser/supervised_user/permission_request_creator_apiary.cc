// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/permission_request_creator_apiary.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;

const int kNumRetries = 1;
const char kIdKey[] = "id";
const char kNamespace[] = "CHROME";
const char kState[] = "PENDING";

static const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

PermissionRequestCreatorApiary::PermissionRequestCreatorApiary(
    OAuth2TokenService* oauth2_token_service,
    scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
    net::URLRequestContextGetter* context)
    : OAuth2TokenService::Consumer("permissions_creator"),
      oauth2_token_service_(oauth2_token_service),
      signin_wrapper_(signin_wrapper.Pass()),
      context_(context),
      access_token_expired_(false) {}

PermissionRequestCreatorApiary::~PermissionRequestCreatorApiary() {}

// static
scoped_ptr<PermissionRequestCreator>
PermissionRequestCreatorApiary::CreateWithProfile(Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper(
      new SupervisedUserSigninManagerWrapper(profile, signin));
  scoped_ptr<PermissionRequestCreator> creator(
      new PermissionRequestCreatorApiary(
          token_service, signin_wrapper.Pass(), profile->GetRequestContext()));
  return creator.Pass();
}

void PermissionRequestCreatorApiary::CreatePermissionRequest(
    const std::string& url_requested,
    const base::Closure& callback) {
  url_requested_ = url_requested;
  callback_ = callback;
  StartFetching();
}

void PermissionRequestCreatorApiary::StartFetching() {
  OAuth2TokenService::ScopeSet scopes;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPermissionRequestApiScope)) {
    scopes.insert(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kPermissionRequestApiScope));
  } else {
    scopes.insert(signin_wrapper_->GetSyncScopeToUse());
  }
  access_token_request_ = oauth2_token_service_->StartRequest(
      signin_wrapper_->GetAccountIdToUse(), scopes, this);
}

void PermissionRequestCreatorApiary::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_ = access_token;
  GURL url(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPermissionRequestApiUrl));
  const int id = 0;

  url_fetcher_.reset(URLFetcher::Create(id, url, URLFetcher::POST, this));

  url_fetcher_->SetRequestContext(context_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  url_fetcher_->AddExtraRequestHeader(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));

  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion("namespace", kNamespace);
  dict.SetStringWithoutPathExpansion("objectRef", url_requested_);
  dict.SetStringWithoutPathExpansion("state", kState);
  std::string body;
  base::JSONWriter::Write(&dict, &body);
  url_fetcher_->SetUploadData("application/json", body);

  url_fetcher_->Start();
}

void PermissionRequestCreatorApiary::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_.get(), request);
  callback_.Run();
  callback_.Reset();
}

void PermissionRequestCreatorApiary::OnURLFetchComplete(
    const URLFetcher* source) {
  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DispatchNetworkError(status.error());
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_UNAUTHORIZED && !access_token_expired_) {
    access_token_expired_ = true;
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(signin_wrapper_->GetSyncScopeToUse());
    oauth2_token_service_->InvalidateToken(
        signin_wrapper_->GetAccountIdToUse(), scopes, access_token_);
    StartFetching();
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    DispatchGoogleServiceAuthError(
        GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  scoped_ptr<base::Value> value(base::JSONReader::Read(response_body));
  base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    DispatchNetworkError(net::ERR_INVALID_RESPONSE);
    return;
  }
  std::string id;
  if (!dict->GetString(kIdKey, &id)) {
    DispatchNetworkError(net::ERR_INVALID_RESPONSE);
    return;
  }
  callback_.Run();
  callback_.Reset();
}

void PermissionRequestCreatorApiary::DispatchNetworkError(int error_code) {
  DispatchGoogleServiceAuthError(
      GoogleServiceAuthError::FromConnectionError(error_code));
}

void PermissionRequestCreatorApiary::DispatchGoogleServiceAuthError(
    const GoogleServiceAuthError& error) {
  callback_.Run();
  callback_.Reset();
}
