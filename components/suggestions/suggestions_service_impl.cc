// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/sync_service.h"
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

using base::TimeDelta;
using base::TimeTicks;

namespace suggestions {

namespace {

// Establishes the different sync states that matter to SuggestionsService.
// There are three different concepts in the sync service: initialized, sync
// enabled and history sync enabled.
enum SyncState {
  // State: Sync service is not initialized, yet not disabled. History sync
  //     state is unknown (since not initialized).
  // Behavior: Does not issue a server request, but serves from cache if
  //     available.
  NOT_INITIALIZED_ENABLED,

  // State: Sync service is initialized, sync is enabled and history sync is
  //     enabled.
  // Behavior: Update suggestions from the server. Serve from cache on timeout.
  INITIALIZED_ENABLED_HISTORY,

  // State: Sync service is disabled or history sync is disabled.
  // Behavior: Do not issue a server request. Clear the cache. Serve empty
  //     suggestions.
  SYNC_OR_HISTORY_SYNC_DISABLED,
};

SyncState GetSyncState(syncer::SyncService* sync) {
  if (!sync || !sync->CanSyncStart() || sync->IsLocalSyncEnabled())
    return SYNC_OR_HISTORY_SYNC_DISABLED;
  if (!sync->IsSyncActive() || !sync->ConfigurationDone())
    return NOT_INITIALIZED_ENABLED;
  return sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES)
             ? INITIALIZED_ENABLED_HISTORY
             : SYNC_OR_HISTORY_SYNC_DISABLED;
}

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
const char kSuggestionsURLFormat[] = "%schromesuggestions?t=%s";
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

// The default expiry timeout is 168 hours.
const int64_t kDefaultExpiryUsec = 168 * base::Time::kMicrosecondsPerHour;

}  // namespace

SuggestionsServiceImpl::SuggestionsServiceImpl(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    syncer::SyncService* sync_service,
    net::URLRequestContextGetter* url_request_context,
    std::unique_ptr<SuggestionsStore> suggestions_store,
    std::unique_ptr<ImageManager> thumbnail_manager,
    std::unique_ptr<BlacklistStore> blacklist_store)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      sync_service_(sync_service),
      sync_service_observer_(this),
      url_request_context_(url_request_context),
      suggestions_store_(std::move(suggestions_store)),
      thumbnail_manager_(std::move(thumbnail_manager)),
      blacklist_store_(std::move(blacklist_store)),
      scheduling_delay_(TimeDelta::FromSeconds(kDefaultSchedulingDelaySec)),
      weak_ptr_factory_(this) {
  // |sync_service_| is null if switches::kDisableSync is set (tests use that).
  if (sync_service_)
    sync_service_observer_.Add(sync_service_);
  // Immediately get the current sync state, so we'll flush the cache if
  // necessary.
  OnStateChanged(sync_service_);
}

SuggestionsServiceImpl::~SuggestionsServiceImpl() {}

bool SuggestionsServiceImpl::FetchSuggestionsData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If sync state allows, issue a network request to refresh the suggestions.
  if (GetSyncState(sync_service_) != INITIALIZED_ENABLED_HISTORY)
    return false;
  IssueRequestIfNoneOngoing(BuildSuggestionsURL());
  return true;
}

base::Optional<SuggestionsProfile>
SuggestionsServiceImpl::GetSuggestionsDataFromCache() const {
  SuggestionsProfile suggestions;
  // In case of empty cache or error, return empty.
  if (!suggestions_store_->LoadSuggestions(&suggestions)) {
    return base::Optional<SuggestionsProfile>();
  }
  thumbnail_manager_->Initialize(suggestions);
  blacklist_store_->FilterSuggestions(&suggestions);
  return base::Optional<SuggestionsProfile>(suggestions);
}

std::unique_ptr<SuggestionsServiceImpl::ResponseCallbackList::Subscription>
SuggestionsServiceImpl::AddCallback(const ResponseCallback& callback) {
  return callback_list_.Add(callback);
}

void SuggestionsServiceImpl::GetPageThumbnail(const GURL& url,
                                              const BitmapCallback& callback) {
  thumbnail_manager_->GetImageForURL(url, callback);
}

void SuggestionsServiceImpl::GetPageThumbnailWithURL(
    const GURL& url,
    const GURL& thumbnail_url,
    const BitmapCallback& callback) {
  thumbnail_manager_->AddImageURL(url, thumbnail_url);
  GetPageThumbnail(url, callback);
}

bool SuggestionsServiceImpl::BlacklistURL(const GURL& candidate_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!blacklist_store_->BlacklistUrl(candidate_url))
    return false;

  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));

  // Blacklist uploads are scheduled on any request completion, so only schedule
  // an upload if there is no ongoing request.
  if (!pending_request_.get())
    ScheduleBlacklistUpload();

  return true;
}

bool SuggestionsServiceImpl::UndoBlacklistURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TimeDelta time_delta;
  if (blacklist_store_->GetTimeUntilURLReadyForUpload(url, &time_delta) &&
      time_delta > TimeDelta::FromSeconds(0) &&
      blacklist_store_->RemoveUrl(url)) {
    // The URL was not yet candidate for upload to the server and could be
    // removed from the blacklist.
    callback_list_.Notify(
        GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));
    return true;
  }
  return false;
}

void SuggestionsServiceImpl::ClearBlacklist() {
  DCHECK(thread_checker_.CalledOnValidThread());
  blacklist_store_->ClearBlacklist();
  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));
  IssueRequestIfNoneOngoing(BuildSuggestionsBlacklistClearURL());
}

// static
bool SuggestionsServiceImpl::GetBlacklistedUrl(const net::URLFetcher& request,
                                               GURL* url) {
  bool is_blacklist_request = base::StartsWith(
      request.GetOriginalURL().spec(), BuildSuggestionsBlacklistURLPrefix(),
      base::CompareCase::SENSITIVE);
  if (!is_blacklist_request)
    return false;

  // Extract the blacklisted URL from the blacklist request.
  std::string blacklisted;
  if (!net::GetValueForKeyInQuery(request.GetOriginalURL(),
                                  kSuggestionsBlacklistURLParam,
                                  &blacklisted)) {
    return false;
  }

  GURL blacklisted_url(blacklisted);
  blacklisted_url.Swap(url);
  return true;
}

// static
void SuggestionsServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsStore::RegisterProfilePrefs(registry);
  BlacklistStore::RegisterProfilePrefs(registry);
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsURL() {
  return GURL(base::StringPrintf(
      kSuggestionsURLFormat, GetGoogleBaseURL().spec().c_str(), kDeviceType));
}

// static
std::string SuggestionsServiceImpl::BuildSuggestionsBlacklistURLPrefix() {
  return base::StringPrintf(kSuggestionsBlacklistURLPrefixFormat,
                            GetGoogleBaseURL().spec().c_str(), kDeviceType);
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsBlacklistURL(
    const GURL& candidate_url) {
  return GURL(BuildSuggestionsBlacklistURLPrefix() +
              net::EscapeQueryParamValue(candidate_url.spec(), true));
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsBlacklistClearURL() {
  return GURL(base::StringPrintf(kSuggestionsBlacklistClearURLFormat,
                                 GetGoogleBaseURL().spec().c_str(),
                                 kDeviceType));
}

void SuggestionsServiceImpl::OnStateChanged(syncer::SyncService* sync) {
  switch (GetSyncState(sync_service_)) {
    case SYNC_OR_HISTORY_SYNC_DISABLED:
      // Cancel any ongoing request, to stop interacting with the server.
      pending_request_.reset(nullptr);
      suggestions_store_->ClearSuggestions();
      callback_list_.Notify(SuggestionsProfile());
      break;
    case NOT_INITIALIZED_ENABLED:
      // Keep the cache (if any), but don't refresh.
      break;
    case INITIALIZED_ENABLED_HISTORY:
      // If we have any observers, issue a network request to refresh the
      // suggestions in the cache.
      if (!callback_list_.empty())
        IssueRequestIfNoneOngoing(BuildSuggestionsURL());
      break;
  }
}

void SuggestionsServiceImpl::SetDefaultExpiryTimestamp(
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

void SuggestionsServiceImpl::IssueRequestIfNoneOngoing(const GURL& url) {
  // If there is an ongoing request, let it complete.
  if (pending_request_.get()) {
    return;
  }
  // If there is an ongoing token request, also wait for that.
  if (token_fetcher_) {
    return;
  }

  OAuth2TokenService::ScopeSet scopes{GaiaConstants::kChromeSyncOAuth2Scope};
  token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "suggestions_service", signin_manager_, token_service_, scopes,
      base::BindOnce(&SuggestionsServiceImpl::AccessTokenAvailable,
                     base::Unretained(this), url));
}

void SuggestionsServiceImpl::AccessTokenAvailable(
    const GURL& url,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK(token_fetcher_);
  std::unique_ptr<AccessTokenFetcher> token_fetcher_deleter(
      std::move(token_fetcher_));

  if (error.state() != GoogleServiceAuthError::NONE) {
    UpdateBlacklistDelay(false);
    ScheduleBlacklistUpload();
    return;
  }

  DCHECK(!access_token.empty());

  IssueSuggestionsRequest(url, access_token);
}

void SuggestionsServiceImpl::IssueSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  DCHECK(!access_token.empty());
  pending_request_ = CreateSuggestionsRequest(url, access_token);
  pending_request_->Start();
  last_request_started_time_ = TimeTicks::Now();
}

std::unique_ptr<net::URLFetcher>
SuggestionsServiceImpl::CreateSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  std::unique_ptr<net::URLFetcher> request =
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      request.get(), data_use_measurement::DataUseUserData::SUGGESTIONS);
  int load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_DO_NOT_SEND_COOKIES |
                   net::LOAD_DO_NOT_SAVE_COOKIES;

  request->SetLoadFlags(load_flags);
  request->SetRequestContext(url_request_context_);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(request->GetOriginalURL(), false, false,
                                     is_signed_in, &headers);
  request->SetExtraRequestHeaders(headers.ToString());
  if (!access_token.empty()) {
    request->AddExtraRequestHeader(
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  }
  return request;
}

void SuggestionsServiceImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(pending_request_.get(), source);

  // The fetcher will be deleted when the request is handled.
  std::unique_ptr<const net::URLFetcher> request(std::move(pending_request_));

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

  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));

  UpdateBlacklistDelay(true);
  ScheduleBlacklistUpload();
}

void SuggestionsServiceImpl::PopulateExtraData(
    SuggestionsProfile* suggestions) {
  for (int i = 0; i < suggestions->suggestions_size(); ++i) {
    suggestions::ChromeSuggestion* s = suggestions->mutable_suggestions(i);
    if (!s->has_favicon_url() || s->favicon_url().empty()) {
      s->set_favicon_url(base::StringPrintf(kFaviconURL, s->url().c_str()));
    }
  }
}

void SuggestionsServiceImpl::Shutdown() {
  // Cancel pending request.
  pending_request_.reset(nullptr);
}

void SuggestionsServiceImpl::ScheduleBlacklistUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TimeDelta time_delta;
  if (blacklist_store_->GetTimeUntilReadyForUpload(&time_delta)) {
    // Blacklist cache is not empty: schedule.
    base::Closure blacklist_cb =
        base::Bind(&SuggestionsServiceImpl::UploadOneFromBlacklist,
                   weak_ptr_factory_.GetWeakPtr());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, blacklist_cb, time_delta + scheduling_delay_);
  }
}

void SuggestionsServiceImpl::UploadOneFromBlacklist() {
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

void SuggestionsServiceImpl::UpdateBlacklistDelay(
    bool last_request_successful) {
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
