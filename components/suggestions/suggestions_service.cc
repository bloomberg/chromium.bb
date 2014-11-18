// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

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
#include "components/variations/net/variations_http_header_provider.h"
#include "components/variations/variations_associated_data.h"
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
  std::vector<SuggestionsService::ResponseCallback> temp_requestors;
  temp_requestors.swap(*requestors);
  std::vector<SuggestionsService::ResponseCallback>::iterator it;
  for (it = temp_requestors.begin(); it != temp_requestors.end(); ++it) {
    if (!it->is_null()) it->Run(suggestions);
  }
}

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
const char kSuggestionsFieldTrialControlParam[] = "control";
const char kSuggestionsFieldTrialStateEnabled[] = "enabled";

// TODO(mathp): Put this in TemplateURL.
const char kSuggestionsURL[] = "https://www.google.com/chromesuggestions?t=2";
const char kSuggestionsBlacklistURLPrefix[] =
    "https://www.google.com/chromesuggestions/blacklist?t=2&url=";
const char kSuggestionsBlacklistURLParam[] = "url";

// The default expiry timeout is 72 hours.
const int64 kDefaultExpiryUsec = 72 * base::Time::kMicrosecondsPerHour;

SuggestionsService::SuggestionsService(
    net::URLRequestContextGetter* url_request_context,
    scoped_ptr<SuggestionsStore> suggestions_store,
    scoped_ptr<ImageManager> thumbnail_manager,
    scoped_ptr<BlacklistStore> blacklist_store)
    : url_request_context_(url_request_context),
      suggestions_store_(suggestions_store.Pass()),
      thumbnail_manager_(thumbnail_manager.Pass()),
      blacklist_store_(blacklist_store.Pass()),
      blacklist_delay_sec_(kBlacklistDefaultDelaySec),
      suggestions_url_(kSuggestionsURL),
      blacklist_url_prefix_(kSuggestionsBlacklistURLPrefix),
      weak_ptr_factory_(this) {}

SuggestionsService::~SuggestionsService() {}

// static
bool SuggestionsService::IsControlGroup() {
  return GetExperimentParam(kSuggestionsFieldTrialControlParam) ==
         kSuggestionsFieldTrialStateEnabled;
}

void SuggestionsService::FetchSuggestionsData(
    SyncState sync_state,
    SuggestionsService::ResponseCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_requestors_.push_back(callback);
  if (sync_state == SYNC_OR_HISTORY_SYNC_DISABLED) {
    // Cancel any ongoing request, to stop interacting with the server.
    pending_request_.reset(NULL);
    suggestions_store_->ClearSuggestions();
    DispatchRequestsAndClear(SuggestionsProfile(), &waiting_requestors_);
  } else if (sync_state == INITIALIZED_ENABLED_HISTORY ||
             sync_state == NOT_INITIALIZED_ENABLED) {
    // Sync is enabled. Serve previously cached suggestions if available, else
    // an empty set of suggestions.
    ServeFromCache();

    // Issue a network request to refresh the suggestions in the cache.
    IssueRequestIfNoneOngoing(suggestions_url_);
  } else {
    NOTREACHED();
  }
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

  // Blacklist locally for immediate effect and serve the requestors.
  blacklist_store_->BlacklistUrl(candidate_url);
  ServeFromCache();

  // Send blacklisting request. Even if this request ends up not being sent
  // because of an ongoing request, a blacklist request is later scheduled.
  // TODO(mathp): Currently, this will not send a request if there is already
  // a request in flight (for suggestions or blacklist). Should we prioritize
  // blacklist requests since they actually carry a payload?
  IssueRequestIfNoneOngoing(
      BuildBlacklistRequestURL(blacklist_url_prefix_, candidate_url));
}

// static
bool SuggestionsService::GetBlacklistedUrl(const net::URLFetcher& request,
                                           GURL* url) {
  bool is_blacklist_request = StartsWithASCII(request.GetOriginalURL().spec(),
                                              kSuggestionsBlacklistURLPrefix,
                                              true);
  if (!is_blacklist_request) return false;

  // Extract the blacklisted URL from the blacklist request.
  std::string blacklisted;
  if (!net::GetValueForKeyInQuery(
          request.GetOriginalURL(),
          kSuggestionsBlacklistURLParam,
          &blacklisted)) {
    return false;
  }

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

void SuggestionsService::IssueRequestIfNoneOngoing(const GURL& url) {
  // If there is an ongoing request, let it complete.
  if (pending_request_.get()) {
    return;
  }
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

void SuggestionsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(pending_request_.get(), source);

  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(pending_request_.release());

  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    ScheduleBlacklistUpload(false);
    return;
  }

  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FetchResponseCode", response_code);
  if (response_code != net::HTTP_OK) {
    // A non-200 response code means that server has no (longer) suggestions for
    // this user. Aggressively clear the cache.
    suggestions_store_->ClearSuggestions();
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

  // Parse the received suggestions and update the cache, or take proper action
  // in the case of invalid response.
  SuggestionsProfile suggestions;
  if (suggestions_data.empty()) {
    LogResponseState(RESPONSE_EMPTY);
    suggestions_store_->ClearSuggestions();
  } else if (suggestions.ParseFromString(suggestions_data)) {
    LogResponseState(RESPONSE_VALID);
    int64 now_usec = (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
        .ToInternalValue();
    SetDefaultExpiryTimestamp(&suggestions, now_usec + kDefaultExpiryUsec);
    suggestions_store_->StoreSuggestions(suggestions);
  } else {
    LogResponseState(RESPONSE_INVALID);
  }

  ScheduleBlacklistUpload(true);
}

void SuggestionsService::Shutdown() {
  // Cancel pending request, then serve existing requestors from cache.
  pending_request_.reset(NULL);
  ServeFromCache();
}

void SuggestionsService::ServeFromCache() {
  SuggestionsProfile suggestions;
  // In case of empty cache or error, |suggestions| stays empty.
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

  GURL blacklist_url;
  if (!blacklist_store_->GetFirstUrlFromBlacklist(&blacklist_url))
    return;  // Local blacklist is empty.

  // Send blacklisting request. Even if this request ends up not being sent
  // because of an ongoing request, a blacklist request is later scheduled.
  IssueRequestIfNoneOngoing(
      BuildBlacklistRequestURL(blacklist_url_prefix_, blacklist_url));
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
