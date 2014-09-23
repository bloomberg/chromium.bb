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

struct PermissionRequestCreatorApiary::Request {
  Request(const GURL& url_requested, const base::Closure& callback);
  ~Request();

  GURL url_requested;
  base::Closure callback;
  scoped_ptr<OAuth2TokenService::Request> access_token_request;
  std::string access_token;
  bool access_token_expired;
  scoped_ptr<net::URLFetcher> url_fetcher;
};

PermissionRequestCreatorApiary::Request::Request(const GURL& url_requested,
                                                 const base::Closure& callback)
    : url_requested(url_requested),
      callback(callback),
      access_token_expired(false) {
}

PermissionRequestCreatorApiary::Request::~Request() {}

PermissionRequestCreatorApiary::PermissionRequestCreatorApiary(
    OAuth2TokenService* oauth2_token_service,
    scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
    net::URLRequestContextGetter* context)
    : OAuth2TokenService::Consumer("permissions_creator"),
      oauth2_token_service_(oauth2_token_service),
      signin_wrapper_(signin_wrapper.Pass()),
      context_(context) {}

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
    const GURL& url_requested,
    const base::Closure& callback) {
  requests_.push_back(new Request(url_requested, callback));
  StartFetching(requests_.back());
}

std::string PermissionRequestCreatorApiary::GetApiScopeToUse() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPermissionRequestApiScope)) {
    return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kPermissionRequestApiScope);
  } else {
    return signin_wrapper_->GetSyncScopeToUse();
  }
}

void PermissionRequestCreatorApiary::StartFetching(Request* request) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GetApiScopeToUse());
  request->access_token_request = oauth2_token_service_->StartRequest(
      signin_wrapper_->GetAccountIdToUse(), scopes, this);
}

void PermissionRequestCreatorApiary::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  RequestIterator it = requests_.begin();
  while (it != requests_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    ++it;
  }
  DCHECK(it != requests_.end());
  (*it)->access_token = access_token;
  GURL url(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPermissionRequestApiUrl));
  const int id = 0;

  (*it)->url_fetcher.reset(URLFetcher::Create(id, url, URLFetcher::POST, this));

  (*it)->url_fetcher->SetRequestContext(context_);
  (*it)->url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                   net::LOAD_DO_NOT_SAVE_COOKIES);
  (*it)->url_fetcher->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  (*it)->url_fetcher->AddExtraRequestHeader(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));

  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion("namespace", kNamespace);
  dict.SetStringWithoutPathExpansion("objectRef", (*it)->url_requested.spec());
  dict.SetStringWithoutPathExpansion("state", kState);
  std::string body;
  base::JSONWriter::Write(&dict, &body);
  (*it)->url_fetcher->SetUploadData("application/json", body);

  (*it)->url_fetcher->Start();
}

void PermissionRequestCreatorApiary::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  RequestIterator it = requests_.begin();
  while (it != requests_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    ++it;
  }
  DCHECK(it != requests_.end());
  (*it)->callback.Run();
  requests_.erase(it);
}

void PermissionRequestCreatorApiary::OnURLFetchComplete(
    const URLFetcher* source) {
  RequestIterator it = requests_.begin();
  while (it != requests_.end()) {
    if (source == (*it)->url_fetcher.get())
      break;
    ++it;
  }
  DCHECK(it != requests_.end());

  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DispatchNetworkError(it, status.error());
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_UNAUTHORIZED && !(*it)->access_token_expired) {
    (*it)->access_token_expired = true;
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(GetApiScopeToUse());
    oauth2_token_service_->InvalidateToken(
        signin_wrapper_->GetAccountIdToUse(), scopes, (*it)->access_token);
    StartFetching(*it);
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    DispatchGoogleServiceAuthError(
        it, GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  scoped_ptr<base::Value> value(base::JSONReader::Read(response_body));
  base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    DispatchNetworkError(it, net::ERR_INVALID_RESPONSE);
    return;
  }
  std::string id;
  if (!dict->GetString(kIdKey, &id)) {
    DispatchNetworkError(it, net::ERR_INVALID_RESPONSE);
    return;
  }
  (*it)->callback.Run();
  requests_.erase(it);
}

void PermissionRequestCreatorApiary::DispatchNetworkError(RequestIterator it,
                                                          int error_code) {
  DispatchGoogleServiceAuthError(
      it, GoogleServiceAuthError::FromConnectionError(error_code));
}

void PermissionRequestCreatorApiary::DispatchGoogleServiceAuthError(
    RequestIterator it,
    const GoogleServiceAuthError& error) {
  (*it)->callback.Run();
  requests_.erase(it);
}
