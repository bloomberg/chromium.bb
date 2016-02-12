// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/suggestions_store.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

using base::CancelableClosure;
using base::TimeDelta;
using base::TimeTicks;

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

// Default delay used when scheduling a request.
const int kDefaultSchedulingDelaySec = 1;

// Multiplier on the delay used when re-scheduling a failed request.
const int kSchedulingBackoffMultiplier = 2;

// Maximum valid delay for scheduling a request. Candidate delays larger than
// this are rejected. This means the maximum backoff is at least 5 / 2 minutes.
const int kSchedulingMaxDelaySec = 5 * 60;

const char kDefaultGoogleBaseURL[] = "https://www.google.com/";

GURL GetGoogleBaseURL() {
  GURL url(google_util::CommandLineGoogleBaseURL());
  if (url.is_valid())
    return url;
  return GURL(kDefaultGoogleBaseURL);
}

// Format strings for the various suggestions URLs. They all have two string
// params: The Google base URL and the device type.
// TODO(mathp): Put this in TemplateURL.
const char kSuggestionsURLFormat[] =
    "%schromesuggestions?t=%s";
const char kSuggestionsBlacklistURLPrefixFormat[] =
    "%schromesuggestions/blacklist?t=%s&url=";
const char kSuggestionsBlacklistClearURLFormat[] =
    "%schromesuggestions/blacklist/clear?t=%s";

const char kSuggestionsBlacklistURLParam[] = "url";

#if defined(OS_ANDROID) || defined(OS_IOS)
const char kDeviceType[] = "2";
#else
const char kDeviceType[] = "1";
#endif

// Format string for OAuth2 authentication headers.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

const char kFaviconURL[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url=%s&alt=s&sz=32";

const char kPingURL[] =
    "https://www.google.com/chromesuggestions/click?q=%lld&cd=%d";

// The default expiry timeout is 168 hours.
const int64_t kDefaultExpiryUsec = 168 * base::Time::kMicrosecondsPerHour;

const base::Feature kOAuth2AuthenticationFeature {
  "SuggestionsServiceOAuth2", base::FEATURE_ENABLED_BY_DEFAULT
};

}  // namespace

// Helper class for fetching OAuth2 access tokens.
// To get a token, call |GetAccessToken|. Does not support multiple concurrent
// token requests, i.e. check |HasPendingRequest| first.
class SuggestionsService::AccessTokenFetcher
    : public OAuth2TokenService::Consumer {
 public:
  using TokenCallback = base::Callback<void(const std::string&)>;

  AccessTokenFetcher(const SigninManagerBase* signin_manager,
                     OAuth2TokenService* token_service)
      : OAuth2TokenService::Consumer("suggestions_service"),
        signin_manager_(signin_manager),
        token_service_(token_service) {}

  void GetAccessToken(const TokenCallback& callback) {
    callback_ = callback;
    std::string account_id;
    // |signin_manager_| can be null in unit tests.
    if (signin_manager_)
      account_id = signin_manager_->GetAuthenticatedAccountId();
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
    token_request_ = token_service_->StartRequest(account_id, scopes, this);
  }

  bool HasPendingRequest() const {
    return !!token_request_.get();
  }

 private:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    DCHECK_EQ(request, token_request_.get());
    callback_.Run(access_token);
    token_request_.reset(nullptr);
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    DCHECK_EQ(request, token_request_.get());
    LOG(WARNING) << "Token error: " << error.ToString();
    callback_.Run(std::string());
    token_request_.reset(nullptr);
  }

  const SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;

  TokenCallback callback_;
  scoped_ptr<OAuth2TokenService::Request> token_request_;
};

SuggestionsService::SuggestionsService(
    const SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* url_request_context,
    scoped_ptr<SuggestionsStore> suggestions_store,
    scoped_ptr<ImageManager> thumbnail_manager,
    scoped_ptr<BlacklistStore> blacklist_store)
    : url_request_context_(url_request_context),
      suggestions_store_(std::move(suggestions_store)),
      thumbnail_manager_(std::move(thumbnail_manager)),
      blacklist_store_(std::move(blacklist_store)),
      scheduling_delay_(TimeDelta::FromSeconds(kDefaultSchedulingDelaySec)),
      token_fetcher_(new AccessTokenFetcher(signin_manager, token_service)),
      weak_ptr_factory_(this) {}

SuggestionsService::~SuggestionsService() {}

void SuggestionsService::FetchSuggestionsData(
    SyncState sync_state,
    const ResponseCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_requestors_.push_back(callback);
  switch (sync_state) {
    case SYNC_OR_HISTORY_SYNC_DISABLED:
      // Cancel any ongoing request, to stop interacting with the server.
      pending_request_.reset(nullptr);
      suggestions_store_->ClearSuggestions();
      DispatchRequestsAndClear(SuggestionsProfile(), &waiting_requestors_);
      break;
    case INITIALIZED_ENABLED_HISTORY:
    case NOT_INITIALIZED_ENABLED:
      // TODO(treib): For NOT_INITIALIZED_ENABLED, we shouldn't issue a network
      // request. Verify that that won't break anything.
      // Sync is enabled. Serve previously cached suggestions if available, else
      // an empty set of suggestions.
      ServeFromCache();

      // Issue a network request to refresh the suggestions in the cache.
      IssueRequestIfNoneOngoing(BuildSuggestionsURL());
      break;
  }
}

void SuggestionsService::GetPageThumbnail(const GURL& url,
                                          const BitmapCallback& callback) {
  thumbnail_manager_->GetImageForURL(url, callback);
}

void SuggestionsService::GetPageThumbnailWithURL(
    const GURL& url,
    const GURL& thumbnail_url,
    const BitmapCallback& callback) {
  thumbnail_manager_->AddImageURL(url, thumbnail_url);
  GetPageThumbnail(url, callback);
}

void SuggestionsService::BlacklistURL(const GURL& candidate_url,
                                      const ResponseCallback& callback,
                                      const base::Closure& fail_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!blacklist_store_->BlacklistUrl(candidate_url)) {
    if (!fail_callback.is_null())
      fail_callback.Run();
    return;
  }

  waiting_requestors_.push_back(callback);
  ServeFromCache();
  // Blacklist uploads are scheduled on any request completion, so only schedule
  // an upload if there is no ongoing request.
  if (!pending_request_.get()) {
    ScheduleBlacklistUpload();
  }
}

void SuggestionsService::UndoBlacklistURL(const GURL& url,
                                          const ResponseCallback& callback,
                                          const base::Closure& fail_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TimeDelta time_delta;
  if (blacklist_store_->GetTimeUntilURLReadyForUpload(url, &time_delta) &&
      time_delta > TimeDelta::FromSeconds(0) &&
      blacklist_store_->RemoveUrl(url)) {
    // The URL was not yet candidate for upload to the server and could be
    // removed from the blacklist.
    waiting_requestors_.push_back(callback);
    ServeFromCache();
    return;
  }
  if (!fail_callback.is_null())
    fail_callback.Run();
}

void SuggestionsService::ClearBlacklist(const ResponseCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  blacklist_store_->ClearBlacklist();
  IssueRequestIfNoneOngoing(BuildSuggestionsBlacklistClearURL());
  waiting_requestors_.push_back(callback);
  ServeFromCache();
}

// static
bool SuggestionsService::GetBlacklistedUrl(const net::URLFetcher& request,
                                           GURL* url) {
  bool is_blacklist_request = base::StartsWith(
      request.GetOriginalURL().spec(), BuildSuggestionsBlacklistURLPrefix(),
      base::CompareCase::SENSITIVE);
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

// static
bool SuggestionsService::UseOAuth2() {
  return base::FeatureList::IsEnabled(kOAuth2AuthenticationFeature);
}

// static
GURL SuggestionsService::BuildSuggestionsURL() {
  return GURL(base::StringPrintf(kSuggestionsURLFormat,
                                 GetGoogleBaseURL().spec().c_str(),
                                 kDeviceType));
}

// static
std::string SuggestionsService::BuildSuggestionsBlacklistURLPrefix() {
  return base::StringPrintf(kSuggestionsBlacklistURLPrefixFormat,
                            GetGoogleBaseURL().spec().c_str(), kDeviceType);
}

// static
GURL SuggestionsService::BuildSuggestionsBlacklistURL(
    const GURL& candidate_url) {
  return GURL(BuildSuggestionsBlacklistURLPrefix() +
              net::EscapeQueryParamValue(candidate_url.spec(), true));
}

// static
GURL SuggestionsService::BuildSuggestionsBlacklistClearURL() {
  return GURL(base::StringPrintf(kSuggestionsBlacklistClearURLFormat,
                                 GetGoogleBaseURL().spec().c_str(),
                                 kDeviceType));
}

void SuggestionsService::SetDefaultExpiryTimestamp(
    SuggestionsProfile* suggestions,
    int64_t default_timestamp_usec) {
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
  if (UseOAuth2()) {
    // If there is an ongoing token request, also wait for that.
    if (token_fetcher_->HasPendingRequest()) {
      return;
    }
    token_fetcher_->GetAccessToken(
        base::Bind(&SuggestionsService::IssueSuggestionsRequest,
                   base::Unretained(this), url));
  } else {
    // No access token required.
    IssueSuggestionsRequest(url, std::string());
  }
}

void SuggestionsService::IssueSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  if (UseOAuth2() && access_token.empty()) {
    UpdateBlacklistDelay(false);
    ScheduleBlacklistUpload();
    return;
  }
  pending_request_ = CreateSuggestionsRequest(url, access_token);
  pending_request_->Start();
  last_request_started_time_ = TimeTicks::Now();
}

scoped_ptr<net::URLFetcher> SuggestionsService::CreateSuggestionsRequest(
    const GURL& url, const std::string& access_token) {
  scoped_ptr<net::URLFetcher> request =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      request.get(), data_use_measurement::DataUseUserData::SUGGESTIONS);
  int load_flags = net::LOAD_DISABLE_CACHE;
  if (UseOAuth2()) {
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  }
  request->SetLoadFlags(load_flags);
  request->SetRequestContext(url_request_context_);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(request->GetOriginalURL(), false, false,
                                     &headers);
  request->SetExtraRequestHeaders(headers.ToString());
  if (UseOAuth2() && !access_token.empty()) {
    request->AddExtraRequestHeader(
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  }
  return request;
}

void SuggestionsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(pending_request_.get(), source);

  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(std::move(pending_request_));

  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    UpdateBlacklistDelay(false);
    ScheduleBlacklistUpload();
    return;
  }

  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Suggestions.FetchResponseCode", response_code);
  if (response_code != net::HTTP_OK) {
    // A non-200 response code means that server has no (longer) suggestions for
    // this user. Aggressively clear the cache.
    suggestions_store_->ClearSuggestions();
    UpdateBlacklistDelay(false);
    ScheduleBlacklistUpload();
    return;
  }

  const TimeDelta latency = TimeTicks::Now() - last_request_started_time_;
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
    int64_t now_usec =
        (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
            .ToInternalValue();
    SetDefaultExpiryTimestamp(&suggestions, now_usec + kDefaultExpiryUsec);
    PopulateExtraData(&suggestions);
    suggestions_store_->StoreSuggestions(suggestions);
  } else {
    LogResponseState(RESPONSE_INVALID);
  }

  UpdateBlacklistDelay(true);
  ScheduleBlacklistUpload();
}

void SuggestionsService::PopulateExtraData(SuggestionsProfile* suggestions) {
  for (int i = 0; i < suggestions->suggestions_size(); ++i) {
    suggestions::ChromeSuggestion* s = suggestions->mutable_suggestions(i);
    if (!s->has_favicon_url() || s->favicon_url().empty()) {
      s->set_favicon_url(base::StringPrintf(kFaviconURL, s->url().c_str()));
    }
    if (!s->has_impression_url() || s->impression_url().empty()) {
      s->set_impression_url(
          base::StringPrintf(
              kPingURL, static_cast<long long>(suggestions->timestamp()), -1));
    }

    if (!s->has_click_url() || s->click_url().empty()) {
      s->set_click_url(base::StringPrintf(
          kPingURL, static_cast<long long>(suggestions->timestamp()), i));
    }
  }
}

void SuggestionsService::Shutdown() {
  // Cancel pending request, then serve existing requestors from cache.
  pending_request_.reset(nullptr);
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

void SuggestionsService::ScheduleBlacklistUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TimeDelta time_delta;
  if (blacklist_store_->GetTimeUntilReadyForUpload(&time_delta)) {
    // Blacklist cache is not empty: schedule.
    base::Closure blacklist_cb =
        base::Bind(&SuggestionsService::UploadOneFromBlacklist,
                   weak_ptr_factory_.GetWeakPtr());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, blacklist_cb, time_delta + scheduling_delay_);
  }
}

void SuggestionsService::UploadOneFromBlacklist() {
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL blacklisted_url;
  if (blacklist_store_->GetCandidateForUpload(&blacklisted_url)) {
    // Issue a blacklisting request. Even if this request ends up not being sent
    // because of an ongoing request, a blacklist request is later scheduled.
    IssueRequestIfNoneOngoing(BuildSuggestionsBlacklistURL(blacklisted_url));
    return;
  }

  // Even though there's no candidate for upload, the blacklist might not be
  // empty.
  ScheduleBlacklistUpload();
}

void SuggestionsService::UpdateBlacklistDelay(bool last_request_successful) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (last_request_successful) {
    scheduling_delay_ = TimeDelta::FromSeconds(kDefaultSchedulingDelaySec);
  } else {
    TimeDelta candidate_delay =
        scheduling_delay_ * kSchedulingBackoffMultiplier;
    if (candidate_delay < TimeDelta::FromSeconds(kSchedulingMaxDelaySec))
      scheduling_delay_ = candidate_delay;
  }
}

}  // namespace suggestions
