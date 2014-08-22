// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/merge_session_helper.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

MergeSessionHelper::ExternalCcResultFetcher::ExternalCcResultFetcher(
    MergeSessionHelper* helper) : helper_(helper) {
  DCHECK(helper_);
}

MergeSessionHelper::ExternalCcResultFetcher::~ExternalCcResultFetcher() {
  CleanupTransientState();
}

std::string MergeSessionHelper::ExternalCcResultFetcher::GetExternalCcResult() {
  std::vector<std::string> results;
  for (ResultMap::const_iterator it = results_.begin(); it != results_.end();
       ++it) {
    results.push_back(it->first + ":" + it->second);
  }
  return JoinString(results, ",");
}

void MergeSessionHelper::ExternalCcResultFetcher::Start() {
  CleanupTransientState();
  results_.clear();
  gaia_auth_fetcher_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          helper_->request_context()));
  gaia_auth_fetcher_->StartGetCheckConnectionInfo();
}

bool MergeSessionHelper::ExternalCcResultFetcher::IsRunning() {
  return gaia_auth_fetcher_ || fetchers_.size() > 0u;
}

void MergeSessionHelper::ExternalCcResultFetcher::TimeoutForTests() {
  Timeout();
}

void
MergeSessionHelper::ExternalCcResultFetcher::OnGetCheckConnectionInfoSuccess(
    const std::string& data) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  const base::ListValue* list;
  if (!value || !value->GetAsList(&list))
    return;

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

  // Some fetches may timeout.  Start a timer to decide when the result fetcher
  // has waited long enough.
  // TODO(rogerta): I have no idea how long to wait before timing out.
  // Gaia folks say this should take no more than 2 second even in mobile.
  // This will need to be tweaked.
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
               this, &MergeSessionHelper::ExternalCcResultFetcher::Timeout);
}

net::URLFetcher* MergeSessionHelper::ExternalCcResultFetcher::CreateFetcher(
    const GURL& url) {
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      0,
      url,
      net::URLFetcher::GET,
      this);
  fetcher->SetRequestContext(helper_->request_context());
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS.
  fetcher->SetAutomaticallyRetryOnNetworkChanges(1);
  return fetcher;
}

void MergeSessionHelper::ExternalCcResultFetcher::OnURLFetchComplete(
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
      timer_.Stop();
      CleanupTransientState();
    }
  }
}

void MergeSessionHelper::ExternalCcResultFetcher::Timeout() {
  CleanupTransientState();
}

void MergeSessionHelper::ExternalCcResultFetcher::CleanupTransientState() {
  gaia_auth_fetcher_.reset();

  for (URLToTokenAndFetcher::const_iterator it = fetchers_.begin();
       it != fetchers_.end(); ++it) {
    delete it->second.second;
  }
  fetchers_.clear();
}

MergeSessionHelper::MergeSessionHelper(
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context,
    Observer* observer)
    : token_service_(token_service),
      request_context_(request_context),
      result_fetcher_(this) {
  if (observer)
    AddObserver(observer);
}

MergeSessionHelper::~MergeSessionHelper() {
  DCHECK(accounts_.empty());
}

void MergeSessionHelper::LogIn(const std::string& account_id) {
  DCHECK(!account_id.empty());
  VLOG(1) << "MergeSessionHelper::LogIn: " << account_id;
  accounts_.push_back(account_id);
  if (accounts_.size() == 1)
    StartFetching();
}

void MergeSessionHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MergeSessionHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MergeSessionHelper::CancelAll() {
  VLOG(1) << "MergeSessionHelper::CancelAll";
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  accounts_.clear();
}

void MergeSessionHelper::LogOut(
    const std::string& account_id,
    const std::vector<std::string>& accounts) {
  DCHECK(!account_id.empty());
  VLOG(1) << "MergeSessionHelper::LogOut: " << account_id
          << " accounts=" << accounts.size();
  LogOutInternal(account_id, accounts);
}

void MergeSessionHelper::LogOutInternal(
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

void MergeSessionHelper::LogOutAllAccounts() {
  VLOG(1) << "MergeSessionHelper::LogOutAllAccounts";
  LogOutInternal("", std::vector<std::string>());
}

void MergeSessionHelper::SignalComplete(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Its possible for the observer to delete |this| object.  Don't access
  // access any members after this calling the observer.  This method should
  // be the last call in any other method.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    MergeSessionCompleted(account_id, error));
}

void MergeSessionHelper::StartFetchingExternalCcResult() {
  result_fetcher_.Start();
}

bool MergeSessionHelper::StillFetchingExternalCcResult() {
  return result_fetcher_.IsRunning();
}

void MergeSessionHelper::StartLogOutUrlFetch() {
  DCHECK(accounts_.front().empty());
  VLOG(1) << "MergeSessionHelper::StartLogOutUrlFetch";
  GURL logout_url(GaiaUrls::GetInstance()->service_logout_url());
  net::URLFetcher* fetcher =
      net::URLFetcher::Create(logout_url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(request_context_);
  fetcher->Start();
}

void MergeSessionHelper::OnUbertokenSuccess(const std::string& uber_token) {
  VLOG(1) << "MergeSessionHelper::OnUbertokenSuccess"
          << " account=" << accounts_.front();
  gaia_auth_fetcher_.reset(new GaiaAuthFetcher(this,
                                               GaiaConstants::kChromeSource,
                                               request_context_));

  // It's possible that not all external checks have completed.
  // GetExternalCcResult() returns results for those that have.
  gaia_auth_fetcher_->StartMergeSession(uber_token,
                                        result_fetcher_.GetExternalCcResult());
}

void MergeSessionHelper::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void MergeSessionHelper::OnMergeSessionSuccess(const std::string& data) {
  VLOG(1) << "MergeSession successful account=" << accounts_.front();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, GoogleServiceAuthError::AuthErrorNone());
}

void MergeSessionHelper::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  HandleNextAccount();
  SignalComplete(account_id, error);
}

void MergeSessionHelper::StartFetching() {
  VLOG(1) << "MergeSessionHelper::StartFetching account_id="
          << accounts_.front();
  uber_token_fetcher_.reset(new UbertokenFetcher(token_service_,
                                                 this,
                                                 request_context_));
  uber_token_fetcher_->StartFetchingToken(accounts_.front());
}

void MergeSessionHelper::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(accounts_.front().empty());
  VLOG(1) << "MergeSessionHelper::OnURLFetchComplete";
  HandleNextAccount();
}

void MergeSessionHelper::HandleNextAccount() {
  VLOG(1) << "MergeSessionHelper::HandleNextAccount";
  accounts_.pop_front();
  gaia_auth_fetcher_.reset();
  if (accounts_.empty()) {
    VLOG(1) << "MergeSessionHelper::HandleNextAccount: no more";
    uber_token_fetcher_.reset();
  } else {
    if (accounts_.front().empty()) {
      StartLogOutUrlFetch();
    } else {
      StartFetching();
    }
  }
}
