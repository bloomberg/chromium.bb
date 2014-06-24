// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using base::CancelableClosure;
using content::BrowserThread;

namespace suggestions {

namespace {

// Used to UMA log the state of the last response from the server.
enum SuggestionsResponseState {
  RESPONSE_EMPTY,
  RESPONSE_INVALID,
  RESPONSE_VALID,
  RESPONSE_STATE_SIZE
};

// Will log the supplied response |state|.
void LogResponseState(SuggestionsResponseState state) {
  UMA_HISTOGRAM_ENUMERATION("Suggestions.ResponseState", state,
                            RESPONSE_STATE_SIZE);
}

// Obtains the experiment parameter under the supplied |key|, or empty string
// if the parameter does not exist.
std::string GetExperimentParam(const std::string& key) {
  return chrome_variations::GetVariationParamValue(kSuggestionsFieldTrialName,
                                                   key);
}

// Runs each callback in |requestors| on |suggestions|, then deallocates
// |requestors|.
void DispatchRequestsAndClear(
    const SuggestionsProfile& suggestions,
    std::vector<SuggestionsService::ResponseCallback>* requestors) {
  std::vector<SuggestionsService::ResponseCallback>::iterator it;
  for (it = requestors->begin(); it != requestors->end(); ++it) {
    it->Run(suggestions);
  }
  std::vector<SuggestionsService::ResponseCallback>().swap(*requestors);
}

const int kDefaultRequestTimeoutMs = 200;

}  // namespace

const char kSuggestionsFieldTrialName[] = "ChromeSuggestions";
const char kSuggestionsFieldTrialURLParam[] = "url";
const char kSuggestionsFieldTrialSuggestionsSuffixParam[] =
    "suggestions_suffix";
const char kSuggestionsFieldTrialBlacklistSuffixParam[] = "blacklist_suffix";
const char kSuggestionsFieldTrialStateParam[] = "state";
const char kSuggestionsFieldTrialControlParam[] = "control";
const char kSuggestionsFieldTrialStateEnabled[] = "enabled";
const char kSuggestionsFieldTrialTimeoutMs[] = "timeout_ms";

SuggestionsService::SuggestionsService(
    Profile* profile, scoped_ptr<SuggestionsStore> suggestions_store,
    scoped_ptr<ThumbnailManager> thumbnail_manager)
    : suggestions_store_(suggestions_store.Pass()),
      thumbnail_manager_(thumbnail_manager.Pass()),
      profile_(profile),
      weak_ptr_factory_(this),
      request_timeout_ms_(kDefaultRequestTimeoutMs) {
  // Obtain various parameters from Variations.
  suggestions_url_ =
      GURL(GetExperimentParam(kSuggestionsFieldTrialURLParam) +
           GetExperimentParam(kSuggestionsFieldTrialSuggestionsSuffixParam));
  blacklist_url_prefix_ =
      GetExperimentParam(kSuggestionsFieldTrialURLParam) +
      GetExperimentParam(kSuggestionsFieldTrialBlacklistSuffixParam);
  std::string timeout = GetExperimentParam(kSuggestionsFieldTrialTimeoutMs);
  int temp_timeout;
  if (!timeout.empty() && base::StringToInt(timeout, &temp_timeout)) {
    request_timeout_ms_ = temp_timeout;
  }
}

SuggestionsService::~SuggestionsService() {}

// static
bool SuggestionsService::IsEnabled() {
  return GetExperimentParam(kSuggestionsFieldTrialStateParam) ==
         kSuggestionsFieldTrialStateEnabled;
}

// static
bool SuggestionsService::IsControlGroup() {
  return GetExperimentParam(kSuggestionsFieldTrialControlParam) ==
         kSuggestionsFieldTrialStateEnabled;
}

void SuggestionsService::FetchSuggestionsData(
    SuggestionsService::ResponseCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FetchSuggestionsDataNoTimeout(callback);

  // Post a task to serve the cached suggestions if the request hasn't completed
  // after some time. Cancels the previous such task, if one existed.
  pending_timeout_closure_.reset(new CancelableClosure(base::Bind(
      &SuggestionsService::OnRequestTimeout, weak_ptr_factory_.GetWeakPtr())));
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE, pending_timeout_closure_->callback(),
      base::TimeDelta::FromMilliseconds(request_timeout_ms_));
}

void SuggestionsService::FetchSuggestionsDataNoTimeout(
    SuggestionsService::ResponseCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (pending_request_.get()) {
    // Request already exists, so just add requestor to queue.
    waiting_requestors_.push_back(callback);
    return;
  }

  // Form new request.
  DCHECK(waiting_requestors_.empty());
  waiting_requestors_.push_back(callback);

  pending_request_.reset(CreateSuggestionsRequest(suggestions_url_));
  pending_request_->Start();
  last_request_started_time_ = base::TimeTicks::Now();
}

void SuggestionsService::GetPageThumbnail(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  thumbnail_manager_->GetPageThumbnail(url, callback);
}

void SuggestionsService::BlacklistURL(
    const GURL& candidate_url, SuggestionsService::ResponseCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  waiting_requestors_.push_back(callback);

  if (pending_request_.get()) {
    if (IsBlacklistRequest(pending_request_.get())) {
      // Pending request is a blacklist request. Silently drop the new blacklist
      // request. TODO - handle this case.
      return;
    } else {
      // Pending request is not a blacklist request - cancel it and go on to
      // issuing a blacklist request. Also ensure the timeout closure does not
      // run; instead we'll wait for the updated suggestions before servicing
      // requestors.
      pending_request_.reset(NULL);
      pending_timeout_closure_.reset(NULL);
    }
  }

  // Send blacklisting request.
  // TODO(manzagop): make this a PUT request instead of a GET request.
  GURL url(blacklist_url_prefix_ +
           net::EscapeQueryParamValue(candidate_url.spec(), true));
  pending_request_.reset(CreateSuggestionsRequest(url));
  pending_request_->Start();
  last_request_started_time_ = base::TimeTicks::Now();
}

// static
void SuggestionsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsStore::RegisterProfilePrefs(registry);
}

void SuggestionsService::OnRequestTimeout() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ServeFromCache();
}

void SuggestionsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(pending_request_.get(), source);

  // We no longer need the timeout closure. Delete it whether or not it has run
  // (if it hasn't, this cancels it).
  pending_timeout_closure_.reset();

  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(pending_request_.release());
  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    // Dispatch the cached profile on error.
    ServeFromCache();
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FetchResponseCode", response_code);
  if (response_code != net::HTTP_OK) {
    // Aggressively clear the store.
    suggestions_store_->ClearSuggestions();
    DispatchRequestsAndClear(SuggestionsProfile(), &waiting_requestors_);
    return;
  }

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Suggestions.FetchSuccessLatency", latency);

  std::string suggestions_data;
  bool success = request->GetResponseAsString(&suggestions_data);
  DCHECK(success);

  // Compute suggestions, and dispatch then to requestors. On error still
  // dispatch empty suggestions.
  SuggestionsProfile suggestions;
  if (suggestions_data.empty()) {
    LogResponseState(RESPONSE_EMPTY);
    suggestions_store_->ClearSuggestions();
  } else if (suggestions.ParseFromString(suggestions_data)) {
    LogResponseState(RESPONSE_VALID);
    thumbnail_manager_->InitializeThumbnailMap(suggestions);
    suggestions_store_->StoreSuggestions(suggestions);
  } else {
    LogResponseState(RESPONSE_INVALID);
    suggestions_store_->LoadSuggestions(&suggestions);
  }

  DispatchRequestsAndClear(suggestions, &waiting_requestors_);
}

void SuggestionsService::Shutdown() {
  // Cancel pending request and timeout closure, then serve existing requestors
  // from cache.
  pending_request_.reset(NULL);
  pending_timeout_closure_.reset(NULL);
  ServeFromCache();
}

bool SuggestionsService::IsBlacklistRequest(net::URLFetcher* request) const {
  DCHECK(request);
  return StartsWithASCII(request->GetOriginalURL().spec(),
                         blacklist_url_prefix_, true);
}

net::URLFetcher* SuggestionsService::CreateSuggestionsRequest(const GURL& url) {
  net::URLFetcher* request =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  request->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request->SetRequestContext(profile_->GetRequestContext());
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  chrome_variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      request->GetOriginalURL(), profile_->IsOffTheRecord(), false, &headers);
  request->SetExtraRequestHeaders(headers.ToString());
  return request;
}

void SuggestionsService::ServeFromCache() {
  SuggestionsProfile suggestions;
  suggestions_store_->LoadSuggestions(&suggestions);
  DispatchRequestsAndClear(suggestions, &waiting_requestors_);
}

}  // namespace suggestions
