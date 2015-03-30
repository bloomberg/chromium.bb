// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

GaiaCookieManagerService::ExternalCcResultFetcher::ExternalCcResultFetcher(
    GaiaCookieManagerService* helper)
    : helper_(helper) {
  DCHECK(helper_);
}

GaiaCookieManagerService::ExternalCcResultFetcher::~ExternalCcResultFetcher() {
  CleanupTransientState();
}

std::string
GaiaCookieManagerService::ExternalCcResultFetcher::GetExternalCcResult() {
  std::vector<std::string> results;
  for (ResultMap::const_iterator it = results_.begin(); it != results_.end();
       ++it) {
    results.push_back(it->first + ":" + it->second);
  }
  return JoinString(results, ",");
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Start() {
  m_external_cc_result_start_time_ = base::Time::Now();

  CleanupTransientState();
  results_.clear();
  gaia_auth_fetcher_.reset(
      new GaiaAuthFetcher(this, helper_->source_, helper_->request_context()));
  gaia_auth_fetcher_->StartGetCheckConnectionInfo();

  // Some fetches may timeout.  Start a timer to decide when the result fetcher
  // has waited long enough.
  // TODO(rogerta): I have no idea how long to wait before timing out.
  // Gaia folks say this should take no more than 2 second even in mobile.
  // This will need to be tweaked.
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5), this,
               &GaiaCookieManagerService::ExternalCcResultFetcher::Timeout);
}

bool GaiaCookieManagerService::ExternalCcResultFetcher::IsRunning() {
  return gaia_auth_fetcher_ || fetchers_.size() > 0u;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::TimeoutForTests() {
  Timeout();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoSuccess(const std::string& data) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  const base::ListValue* list;
  if (!value || !value->GetAsList(&list)) {
    CleanupTransientState();
    FireGetCheckConnectionInfoCompleted(false);
    return;
  }

  // If there is nothing to check, terminate immediately.
  if (list->GetSize() == 0) {
    CleanupTransientState();
    FireGetCheckConnectionInfoCompleted(true);
    return;
  }

  // Start a fetcher for each connection URL that needs to be checked.
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      std::string token;
      std::string url;
      if (dict->GetString("carryBackToken", &token) &&
          dict->GetString("url", &url)) {
        results_[token] = "null";
        net::URLFetcher* fetcher = CreateFetcher(GURL(url));
        fetchers_[fetcher->GetOriginalURL()] = std::make_pair(token, fetcher);
        fetcher->Start();
      }
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoError(const GoogleServiceAuthError& error) {
  CleanupTransientState();
  FireGetCheckConnectionInfoCompleted(false);
}

net::URLFetcher*
GaiaCookieManagerService::ExternalCcResultFetcher::CreateFetcher(
    const GURL& url) {
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(helper_->request_context());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS.
  fetcher->SetAutomaticallyRetryOnNetworkChanges(1);
  return fetcher;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  const GURL& url = source->GetOriginalURL();
  const net::URLRequestStatus& status = source->GetStatus();
  int response_code = source->GetResponseCode();
  if (status.is_success() && response_code == net::HTTP_OK &&
      fetchers_.count(url) > 0) {
    std::string data;
    source->GetResponseAsString(&data);
    // Only up to the first 16 characters of the response are important to GAIA.
    // Truncate if needed to keep amount data sent back to GAIA down.
    if (data.size() > 16)
      data.resize(16);
    results_[fetchers_[url].first] = data;

    // Clean up tracking of this fetcher.  The rest will be cleaned up after
    // the timer expires in CleanupTransientState().
    DCHECK_EQ(source, fetchers_[url].second);
    fetchers_.erase(url);
    delete source;

    // If all expected responses have been received, cancel the timer and
    // report the result.
    if (fetchers_.empty()) {
      CleanupTransientState();
      FireGetCheckConnectionInfoCompleted(true);
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Timeout() {
  CleanupTransientState();
  FireGetCheckConnectionInfoCompleted(false);
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    CleanupTransientState() {
  timer_.Stop();
  gaia_auth_fetcher_.reset();

  for (URLToTokenAndFetcher::const_iterator it = fetchers_.begin();
       it != fetchers_.end(); ++it) {
    delete it->second.second;
  }
  fetchers_.clear();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    FireGetCheckConnectionInfoCompleted(bool succeeded) {
  base::TimeDelta time_to_check_connections =
      base::Time::Now() - m_external_cc_result_start_time_;
  signin_metrics::LogExternalCcResultFetches(succeeded,
                                             time_to_check_connections);
  FOR_EACH_OBSERVER(Observer, helper_->observer_list_,
                    GetCheckConnectionInfoCompleted(succeeded));
}

GaiaCookieManagerService::GaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* signin_client)
    : token_service_(token_service),
      signin_client_(signin_client),
      external_cc_result_fetcher_(this),
      source_(source) {
}

GaiaCookieManagerService::~GaiaCookieManagerService() {
  CancelAll();
  DCHECK(accounts_.empty());
}

void GaiaCookieManagerService::AddAccountToCookie(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  VLOG(1) << "GaiaCookieManagerService::AddAccountToCookie: " << account_id;
  accounts_.push_back(account_id);
  if (accounts_.size() == 1)
    StartFetching();
}

void GaiaCookieManagerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GaiaCookieManagerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GaiaCookieManagerService::CancelAll() {
  VLOG(1) << "GaiaCookieManagerService::CancelAll";
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  accounts_.clear();
}

void GaiaCookieManagerService::LogOut(
    const std::string& account_id,
    const std::vector<std::string>& accounts) {
  DCHECK(!account_id.empty());
  VLOG(1) << "GaiaCookieManagerService::LogOut: " << account_id
          << " accounts=" << accounts.size();
  LogOutInternal(account_id, accounts);
}

void GaiaCookieManagerService::LogOutInternal(
    const std::string& account_id,
    const std::vector<std::string>& accounts) {
  bool pending = !accounts_.empty();

  if (pending) {
    for (std::deque<std::string>::const_iterator it = accounts_.begin() + 1;
         it != accounts_.end(); it++) {
      if (!it->empty() &&
          (std::find(accounts.begin(), accounts.end(), *it) == accounts.end() ||
           *it == account_id)) {
        // We have a pending log in request for an account followed by
        // a signout.
        GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
        SignalComplete(*it, error);
      }
    }

    // Remove every thing in the work list besides the one that is running.
    accounts_.resize(1);
  }

  // Signal a logout to be the next thing to do unless the pending
  // action is already a logout.
  if (!pending || !accounts_.front().empty())
    accounts_.push_back("");

  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); it++) {
    if (*it != account_id) {
      DCHECK(!it->empty());
      accounts_.push_back(*it);
    }
  }

  if (!pending)
    StartLogOutUrlFetch();
}

void GaiaCookieManagerService::LogOutAllAccounts() {
  VLOG(1) << "GaiaCookieManagerService::LogOutAllAccounts";
  LogOutInternal("", std::vector<std::string>());
}

void GaiaCookieManagerService::SignalComplete(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Its possible for the observer to delete |this| object.  Don't access
  // access any members after this calling the observer.  This method should
  // be the last call in any other method.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnAddAccountToCookieCompleted(account_id, error));
}

void GaiaCookieManagerService::StartFetchingExternalCcResult() {
  if (!external_cc_result_fetcher_.IsRunning())
    external_cc_result_fetcher_.Start();
}

void GaiaCookieManagerService::StartLogOutUrlFetch() {
  DCHECK(accounts_.front().empty());
  VLOG(1) << "GaiaCookieManagerService::StartLogOutUrlFetch";
  GURL logout_url(GaiaUrls::GetInstance()->service_logout_url().Resolve(
      base::StringPrintf("?source=%s", source_.c_str())));
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(logout_url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(signin_client_->GetURLRequestContext());
  fetcher->Start();
}

void GaiaCookieManagerService::OnUbertokenSuccess(
    const std::string& uber_token) {
  VLOG(1) << "GaiaCookieManagerService::OnUbertokenSuccess"
          << " account=" << accounts_.front();
  gaia_auth_fetcher_.reset(
      new GaiaAuthFetcher(this, source_,
                          signin_client_->GetURLRequestContext()));

  // It's possible that not all external checks have completed.
  // GetExternalCcResult() returns results for those that have.
  gaia_auth_fetcher_->StartMergeSession(uber_token,
      external_cc_result_fetcher_.GetExternalCcResult());
}

void GaiaCookieManagerService::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken"
          << " account=" << accounts_.front() << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void GaiaCookieManagerService::OnMergeSessionSuccess(const std::string& data) {
  VLOG(1) << "MergeSession successful account=" << accounts_.front();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, GoogleServiceAuthError::AuthErrorNone());
}

void GaiaCookieManagerService::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession"
          << " account=" << accounts_.front() << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void GaiaCookieManagerService::StartFetching() {
  VLOG(1) << "GaiaCookieManagerService::StartFetching account_id="
          << accounts_.front();
  uber_token_fetcher_.reset(
      new UbertokenFetcher(token_service_, this, source_,
                           signin_client_->GetURLRequestContext()));
  uber_token_fetcher_->StartFetchingToken(accounts_.front());
}

void GaiaCookieManagerService::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(accounts_.front().empty());
  VLOG(1) << "GaiaCookieManagerService::OnURLFetchComplete";
  HandleNextAccount();
}

void GaiaCookieManagerService::HandleNextAccount() {
  VLOG(1) << "GaiaCookieManagerService::HandleNextAccount";
  accounts_.pop_front();
  gaia_auth_fetcher_.reset();
  if (accounts_.empty()) {
    VLOG(1) << "GaiaCookieManagerService::HandleNextAccount: no more";
    uber_token_fetcher_.reset();
  } else {
    if (accounts_.front().empty()) {
      StartLogOutUrlFetch();
    } else {
      StartFetching();
    }
  }
}
