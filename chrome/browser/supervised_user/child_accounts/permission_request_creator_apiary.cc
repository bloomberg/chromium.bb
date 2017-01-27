// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/kids_management_api.h"
#include "chrome/common/chrome_switches.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using net::URLFetcher;

const char kApiPath[] = "people/me/permissionRequests";
const char kApiScope[] = "https://www.googleapis.com/auth/kid.permission";

const int kNumRetries = 1;

const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

// Request keys.
const char kEventTypeKey[] = "eventType";
const char kObjectRefKey[] = "objectRef";
const char kStateKey[] = "state";

// Request values.
const char kEventTypeURLRequest[] = "PERMISSION_CHROME_URL";
const char kEventTypeInstallRequest[] = "PERMISSION_CHROME_CWS_ITEM_INSTALL";
const char kEventTypeUpdateRequest[] = "PERMISSION_CHROME_CWS_ITEM_UPDATE";
const char kState[] = "PENDING";

// Response keys.
const char kPermissionRequestKey[] = "permissionRequest";
const char kIdKey[] = "id";

struct PermissionRequestCreatorApiary::Request {
  Request(const std::string& request_type,
          const std::string& object_ref,
          const SuccessCallback& callback,
          int url_fetcher_id);
  ~Request();

  std::string request_type;
  std::string object_ref;
  SuccessCallback callback;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request;
  std::string access_token;
  bool access_token_expired;
  int url_fetcher_id;
  std::unique_ptr<URLFetcher> url_fetcher;
};

PermissionRequestCreatorApiary::Request::Request(
    const std::string& request_type,
    const std::string& object_ref,
    const SuccessCallback& callback,
    int url_fetcher_id)
    : request_type(request_type),
      object_ref(object_ref),
      callback(callback),
      access_token_expired(false),
      url_fetcher_id(url_fetcher_id) {
}

PermissionRequestCreatorApiary::Request::~Request() {}

PermissionRequestCreatorApiary::PermissionRequestCreatorApiary(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    net::URLRequestContextGetter* context)
    : OAuth2TokenService::Consumer("permissions_creator"),
      oauth2_token_service_(oauth2_token_service),
      account_id_(account_id),
      context_(context),
      url_fetcher_id_(0) {
}

PermissionRequestCreatorApiary::~PermissionRequestCreatorApiary() {}

// static
std::unique_ptr<PermissionRequestCreator>
PermissionRequestCreatorApiary::CreateWithProfile(Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return base::WrapUnique(new PermissionRequestCreatorApiary(
      token_service, signin->GetAuthenticatedAccountId(),
      profile->GetRequestContext()));
}

bool PermissionRequestCreatorApiary::IsEnabled() const {
  return true;
}

void PermissionRequestCreatorApiary::CreateURLAccessRequest(
    const GURL& url_requested,
    const SuccessCallback& callback) {
  CreateRequest(kEventTypeURLRequest, url_requested.spec(), callback);
}

void PermissionRequestCreatorApiary::CreateExtensionInstallRequest(
    const std::string& id,
    const SuccessCallback& callback) {
  CreateRequest(kEventTypeInstallRequest, id, callback);
}

void PermissionRequestCreatorApiary::CreateExtensionUpdateRequest(
    const std::string& id,
    const SuccessCallback& callback) {
  CreateRequest(kEventTypeUpdateRequest, id, callback);
}

GURL PermissionRequestCreatorApiary::GetApiUrl() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPermissionRequestApiUrl)) {
    GURL url(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kPermissionRequestApiUrl));
    LOG_IF(WARNING, !url.is_valid())
        << "Got invalid URL for " << switches::kPermissionRequestApiUrl;
    return url;
  }

  return kids_management_api::GetURL(kApiPath);
}

std::string PermissionRequestCreatorApiary::GetApiScope() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPermissionRequestApiScope)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kPermissionRequestApiScope);
  } else {
    return kApiScope;
  }
}

void PermissionRequestCreatorApiary::CreateRequest(
    const std::string& request_type,
    const std::string& object_ref,
    const SuccessCallback& callback) {
  requests_.push_back(
      new Request(request_type, object_ref, callback, url_fetcher_id_));
  StartFetching(requests_.back());
}

void PermissionRequestCreatorApiary::StartFetching(Request* request) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GetApiScope());
  request->access_token_request = oauth2_token_service_->StartRequest(
      account_id_, scopes, this);
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

  (*it)->url_fetcher = URLFetcher::Create((*it)->url_fetcher_id, GetApiUrl(),
                                          URLFetcher::POST, this);

  data_use_measurement::DataUseUserData::AttachToFetcher(
      (*it)->url_fetcher.get(),
      data_use_measurement::DataUseUserData::SUPERVISED_USER);
  (*it)->url_fetcher->SetRequestContext(context_);
  (*it)->url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                   net::LOAD_DO_NOT_SAVE_COOKIES);
  (*it)->url_fetcher->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);
  (*it)->url_fetcher->AddExtraRequestHeader(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));

  base::DictionaryValue dict;
  dict.SetStringWithoutPathExpansion(kEventTypeKey, (*it)->request_type);
  dict.SetStringWithoutPathExpansion(kObjectRefKey, (*it)->object_ref);
  dict.SetStringWithoutPathExpansion(kStateKey, kState);

  std::string body;
  base::JSONWriter::Write(dict, &body);
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
  LOG(WARNING) << "Token error: " << error.ToString();
  DispatchResult(it, false);
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
    LOG(WARNING) << "Network error " << status.error();
    DispatchResult(it, false);
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code == net::HTTP_UNAUTHORIZED && !(*it)->access_token_expired) {
    (*it)->access_token_expired = true;
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(GetApiScope());
    oauth2_token_service_->InvalidateAccessToken(account_id_, scopes,
                                                 (*it)->access_token);
    StartFetching(*it);
    return;
  }

  if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "HTTP error " << response_code;
    DispatchResult(it, false);
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response_body);
  base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    LOG(WARNING) << "Invalid top-level dictionary";
    DispatchResult(it, false);
    return;
  }
  base::DictionaryValue* permission_dict = NULL;
  if (!dict->GetDictionary(kPermissionRequestKey, &permission_dict)) {
    LOG(WARNING) << "Permission request not found";
    DispatchResult(it, false);
    return;
  }
  std::string id;
  if (!permission_dict->GetString(kIdKey, &id)) {
    LOG(WARNING) << "ID not found";
    DispatchResult(it, false);
    return;
  }
  DispatchResult(it, true);
}

void PermissionRequestCreatorApiary::DispatchResult(RequestIterator it,
                                                    bool success) {
  (*it)->callback.Run(success);
  requests_.erase(it);
}
