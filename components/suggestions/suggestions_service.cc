// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

#include <sstream>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/suggestions_store.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_http_header_provider.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using base::CancelableClosure;

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
  return variations::GetVariationParamValue(kSuggestionsFieldTrialName, key);
}

GURL BuildBlacklistRequestURL(const std::string& blacklist_url_prefix,
                              const GURL& candidate_url) {
  return GURL(blacklist_url_prefix +
              net::EscapeQueryParamValue(candidate_url.spec(), true));
}

// Runs each callback in |requestors| on |suggestions|, then deallocates
// |requestors|.
void DispatchRequestsAndClear(
    const SuggestionsProfile& suggestions,
    std::vector<SuggestionsService::ResponseCallback>* requestors) {
  std::vector<SuggestionsService::ResponseCallback>::iterator it;
  for (it = requestors->begin(); it != requestors->end(); ++it) {
    if (!it->is_null()) it->Run(suggestions);
  }
  std::vector<SuggestionsService::ResponseCallback>().swap(*requestors);
}

const int kDefaultRequestTimeoutMs = 200;

// Default delay used when scheduling a blacklist request.
const int kBlacklistDefaultDelaySec = 1;

// Multiplier on the delay used when scheduling a blacklist request, in case the
// last observed request was unsuccessful.
const int kBlacklistBackoffMultiplier = 2;

// Maximum valid delay for scheduling a request. Candidate delays larger than
// this are rejected. This means the maximum backoff is at least 300 / 2, i.e.
// 2.5 minutes.
const int kBlacklistMaxDelaySec = 300;  // 5 minutes

}  // namespace

const char kSuggestionsFieldTrialName[] = "ChromeSuggestions";
const char kSuggestionsFieldTrialURLParam[] = "url";
const char kSuggestionsFieldTrialCommonParamsParam[] = "common_params";
const char kSuggestionsFieldTrialBlacklistPathParam[] = "blacklist_path";
const char kSuggestionsFieldTrialBlacklistUrlParam[] = "blacklist_url_param";
const char kSuggestionsFieldTrialStateParam[] = "state";
const char kSuggestionsFieldTrialControlParam[] = "control";
const char kSuggestionsFieldTrialStateEnabled[] = "enabled";
const char kSuggestionsFieldTrialTimeoutMs[] = "timeout_ms";

// The default expiry timeout is 72 hours.
const int64 kDefaultExpiryUsec = 72 * base::Time::kMicrosecondsPerHour;

namespace {

std::string GetBlacklistUrlPrefix() {
  std::stringstream blacklist_url_prefix_stream;
  blacklist_url_prefix_stream
      << GetExperimentParam(kSuggestionsFieldTrialURLParam)
      << GetExperimentParam(kSuggestionsFieldTrialBlacklistPathParam) << "?"
      << GetExperimentParam(kSuggestionsFieldTrialCommonParamsParam) << "&"
      << GetExperimentParam(kSuggestionsFieldTrialBlacklistUrlParam) << "=";
  return blacklist_url_prefix_stream.str();
}

}  // namespace

SuggestionsService::SuggestionsService(
    net::URLRequestContextGetter* url_request_context,
    scoped_ptr<SuggestionsStore> suggestions_store,
    scoped_ptr<ImageManager> thumbnail_manager,
    scoped_ptr<BlacklistStore> blacklist_store)
    : suggestions_store_(suggestions_store.Pass()),
      blacklist_store_(blacklist_store.Pass()),
      thumbnail_manager_(thumbnail_manager.Pass()),
      url_request_context_(url_request_context),
      blacklist_delay_sec_(kBlacklistDefaultDelaySec),
      weak_ptr_factory_(this),
      request_timeout_ms_(kDefaultRequestTimeoutMs) {
  // Obtain various parameters from Variations.
  suggestions_url_ =
      GURL(GetExperimentParam(kSuggestionsFieldTrialURLParam) + "?" +
           GetExperimentParam(kSuggestionsFieldTrialCommonParamsParam));
  blacklist_url_prefix_ = GetBlacklistUrlPrefix();
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
  DCHECK(thread_checker_.CalledOnValidThread());

  FetchSuggestionsDataNoTimeout(callback);

  // Post a task to serve the cached suggestions if the request hasn't completed
  // after some time. Cancels the previous such task, if one existed.
  pending_timeout_closure_.reset(new CancelableClosure(base::Bind(
      &SuggestionsService::OnRequestTimeout, weak_ptr_factory_.GetWeakPtr())));
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE, pending_timeout_closure_->callback(),
      base::TimeDelta::FromMilliseconds(request_timeout_ms_));
}

void SuggestionsService::FetchSuggestionsDataNoTimeout(
    SuggestionsService::ResponseCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pending_request_.get()) {
    // Request already exists, so just add requestor to queue.
    waiting_requestors_.push_back(callback);
    return;
  }

  // Form new request.
  DCHECK(waiting_requestors_.empty());
  waiting_requestors_.push_back(callback);
  IssueRequest(suggestions_url_);
}

void SuggestionsService::GetPageThumbnail(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  thumbnail_manager_->GetImageForURL(url, callback);
}

void SuggestionsService::BlacklistURL(
    const GURL& candidate_url,
    const SuggestionsService::ResponseCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_requestors_.push_back(callback);

  // Blacklist locally, for immediate effect.
  if (!blacklist_store_->BlacklistUrl(candidate_url)) {
    DVLOG(1) << "Failed blacklisting attempt.";
    return;
  }

  // If there's an ongoing request, let it complete.
  if (pending_request_.get()) return;
  IssueRequest(BuildBlacklistRequestURL(blacklist_url_prefix_, candidate_url));
}

// static
bool SuggestionsService::GetBlacklistedUrl(const net::URLFetcher& request,
                                           GURL* url) {
  bool is_blacklist_request = StartsWithASCII(request.GetOriginalURL().spec(),
                                              GetBlacklistUrlPrefix(), true);
  if (!is_blacklist_request) return false;

  // Extract the blacklisted URL from the blacklist request.
  std::string blacklisted;
  if (!net::GetValueForKeyInQuery(
          request.GetOriginalURL(),
          GetExperimentParam(kSuggestionsFieldTrialBlacklistUrlParam),
          &blacklisted))
    return false;

  GURL blacklisted_url(blacklisted);
  blacklisted_url.Swap(url);
  return true;
}

// static
void SuggestionsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsStore::RegisterProfilePrefs(registry);
  BlacklistStore::RegisterProfilePrefs(registry);
}

void SuggestionsService::IssueRequest(const GURL& url) {
  pending_request_.reset(CreateSuggestionsRequest(url));
  pending_request_->Start();
  last_request_started_time_ = base::TimeTicks::Now();
}

net::URLFetcher* SuggestionsService::CreateSuggestionsRequest(const GURL& url) {
  net::URLFetcher* request =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  request->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  request->SetRequestContext(url_request_context_);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
      request->GetOriginalURL(), false, false, &headers);
  request->SetExtraRequestHeaders(headers.ToString());
  return request;
}

void SuggestionsService::OnRequestTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ServeFromCache();
}

void SuggestionsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(pending_request_.get(), source);
  // We no longer need the timeout closure. Delete it whether or not it has run.
  // If it hasn't, this cancels it.
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
    ScheduleBlacklistUpload(false);
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FetchResponseCode", response_code);
  if (response_code != net::HTTP_OK) {
    // Aggressively clear the store.
    suggestions_store_->ClearSuggestions();
    DispatchRequestsAndClear(SuggestionsProfile(), &waiting_requestors_);
    ScheduleBlacklistUpload(false);
    return;
  }

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Suggestions.FetchSuccessLatency", latency);

  // Handle a successful blacklisting.
  GURL blacklisted_url;
  if (GetBlacklistedUrl(*source, &blacklisted_url)) {
    blacklist_store_->RemoveUrl(blacklisted_url);
  }

  std::string suggestions_data;
  bool success = request->GetResponseAsString(&suggestions_data);
  DCHECK(success);

  // Compute suggestions, and dispatch them to requestors. On error still
  // dispatch empty suggestions.
  SuggestionsProfile suggestions;
  if (suggestions_data.empty()) {
    LogResponseState(RESPONSE_EMPTY);
    suggestions_store_->ClearSuggestions();
  } else if (suggestions.ParseFromString(suggestions_data)) {
    LogResponseState(RESPONSE_VALID);
    thumbnail_manager_->Initialize(suggestions);

    int64 now_usec = (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
        .ToInternalValue();
    SetDefaultExpiryTimestamp(&suggestions, now_usec + kDefaultExpiryUsec);
    suggestions_store_->StoreSuggestions(suggestions);
  } else {
    LogResponseState(RESPONSE_INVALID);
    suggestions_store_->LoadSuggestions(&suggestions);
    thumbnail_manager_->Initialize(suggestions);
  }

  FilterAndServe(&suggestions);
  ScheduleBlacklistUpload(true);
}

void SuggestionsService::SetDefaultExpiryTimestamp(
    SuggestionsProfile* suggestions, int64 default_timestamp_usec) {
  for (int i = 0; i < suggestions->suggestions_size(); ++i) {
    ChromeSuggestion* suggestion = suggestions->mutable_suggestions(i);
    // Do not set expiry if the server has already provided a more specific
    // expiry time for this suggestion.
    if (!suggestion->has_expiry_ts()) {
      suggestion->set_expiry_ts(default_timestamp_usec);
    }
  }
}

void SuggestionsService::Shutdown() {
  // Cancel pending request and timeout closure, then serve existing requestors
  // from cache.
  pending_request_.reset(NULL);
  pending_timeout_closure_.reset(NULL);
  ServeFromCache();
}

void SuggestionsService::ServeFromCache() {
  SuggestionsProfile suggestions;
  suggestions_store_->LoadSuggestions(&suggestions);
  thumbnail_manager_->Initialize(suggestions);
  FilterAndServe(&suggestions);
}

void SuggestionsService::FilterAndServe(SuggestionsProfile* suggestions) {
  blacklist_store_->FilterSuggestions(suggestions);
  DispatchRequestsAndClear(*suggestions, &waiting_requestors_);
}

void SuggestionsService::ScheduleBlacklistUpload(bool last_request_successful) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UpdateBlacklistDelay(last_request_successful);

  // Schedule a blacklist upload task.
  GURL blacklist_url;
  if (blacklist_store_->GetFirstUrlFromBlacklist(&blacklist_url)) {
    base::Closure blacklist_cb =
        base::Bind(&SuggestionsService::UploadOneFromBlacklist,
                   weak_ptr_factory_.GetWeakPtr());
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE, blacklist_cb,
        base::TimeDelta::FromSeconds(blacklist_delay_sec_));
  }
}

void SuggestionsService::UploadOneFromBlacklist() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If there's an ongoing request, let it complete.
  if (pending_request_.get()) return;

  GURL blacklist_url;
  if (!blacklist_store_->GetFirstUrlFromBlacklist(&blacklist_url))
    return;  // Local blacklist is empty.

  // Send blacklisting request.
  IssueRequest(BuildBlacklistRequestURL(blacklist_url_prefix_, blacklist_url));
}

void SuggestionsService::UpdateBlacklistDelay(bool last_request_successful) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (last_request_successful) {
    blacklist_delay_sec_ = kBlacklistDefaultDelaySec;
  } else {
    int candidate_delay = blacklist_delay_sec_ * kBlacklistBackoffMultiplier;
    if (candidate_delay < kBlacklistMaxDelaySec)
      blacklist_delay_sec_ = candidate_delay;
  }
}

}  // namespace suggestions
