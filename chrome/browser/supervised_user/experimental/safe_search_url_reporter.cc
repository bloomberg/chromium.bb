// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
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

const char kApiUrl[] = "https://safesearch.googleapis.com/v1:report";
const char kApiScope[] = "https://www.googleapis.com/auth/safesearch.reporting";

const int kNumRetries = 1;

const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

// Request keys
const char kUrlKey[] = "url";

struct SafeSearchURLReporter::Report {
  Report(const GURL& url, const SuccessCallback& callback, int url_fetcher_id);
  ~Report();

  GURL url;
  SuccessCallback callback;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request;
  std::string access_token;
  bool access_token_expired;
  int url_fetcher_id;
  std::unique_ptr<URLFetcher> url_fetcher;
};

SafeSearchURLReporter::Report::Report(const GURL& url,
                                      const SuccessCallback& callback,
                                      int url_fetcher_id)
    : url(url),
      callback(callback),
      access_token_expired(false),
      url_fetcher_id(url_fetcher_id) {}

SafeSearchURLReporter::Report::~Report() {}

SafeSearchURLReporter::SafeSearchURLReporter(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    net::URLRequestContextGetter* context)
    : OAuth2TokenService::Consumer("safe_search_url_reporter"),
      oauth2_token_service_(oauth2_token_service),
      account_id_(account_id),
      context_(context),
      url_fetcher_id_(0) {}

SafeSearchURLReporter::~SafeSearchURLReporter() {}

// static
std::unique_ptr<SafeSearchURLReporter> SafeSearchURLReporter::CreateWithProfile(
    Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return base::WrapUnique(new SafeSearchURLReporter(
      token_service, signin->GetAuthenticatedAccountId(),
      profile->GetRequestContext()));
}

void SafeSearchURLReporter::ReportUrl(const GURL& url,
                                      const SuccessCallback& callback) {
  reports_.push_back(base::MakeUnique<Report>(url, callback, url_fetcher_id_));
  StartFetching(reports_.back().get());
}

void SafeSearchURLReporter::StartFetching(Report* report) {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kApiScope);
  report->access_token_request =
      oauth2_token_service_->StartRequest(account_id_, scopes, this);
}

void SafeSearchURLReporter::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  ReportIterator it = reports_.begin();
  while (it != reports_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    it++;
  }
  DCHECK(it != reports_.end());

  (*it)->access_token = access_token;

  (*it)->url_fetcher = URLFetcher::Create((*it)->url_fetcher_id, GURL(kApiUrl),
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
  dict.SetStringWithoutPathExpansion(kUrlKey, (*it)->url.spec().c_str());

  std::string body;
  base::JSONWriter::Write(dict, &body);
  (*it)->url_fetcher->SetUploadData("application/json", body);

  (*it)->url_fetcher->Start();
}

void SafeSearchURLReporter::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  ReportIterator it = reports_.begin();
  while (it != reports_.end()) {
    if (request == (*it)->access_token_request.get())
      break;
    it++;
  }
  DCHECK(it != reports_.end());
  LOG(WARNING) << "Token error: " << error.ToString();
  DispatchResult(it, false);
}

void SafeSearchURLReporter::OnURLFetchComplete(const URLFetcher* source) {
  ReportIterator it = reports_.begin();
  while (it != reports_.end()) {
    if (source == (*it)->url_fetcher.get())
      break;
    ++it;
  }
  DCHECK(it != reports_.end());

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
    scopes.insert(kApiScope);
    oauth2_token_service_->InvalidateAccessToken(account_id_, scopes,
                                                 (*it)->access_token);
    StartFetching((*it).get());
    return;
  }

  if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "HTTP error " << response_code;
    DispatchResult(it, false);
    return;
  }

  DispatchResult(it, true);
}

void SafeSearchURLReporter::DispatchResult(ReportIterator it, bool success) {
  (*it)->callback.Run(success);
  reports_.erase(it);
}
