// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/features.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/sync_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/identity/public/cpp/identity_manager.h"

using base::TimeDelta;

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

const net::BackoffEntry::Policy kBlacklistBackoffPolicy = {
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/1000,
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.0,
    /*maximum_backoff_ms=*/2 * 60 * 60 * 1000,
    /*entry_lifetime_ms=*/-1,
    /*always_use_initial_delay=*/true};

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
const char kSuggestionsURLFormat[] = "%schromesuggestions?%s";
const char kSuggestionsBlacklistURLPrefixFormat[] =
    "%schromesuggestions/blacklist?t=%s&url=";
const char kSuggestionsBlacklistClearURLFormat[] =
    "%schromesuggestions/blacklist/clear?t=%s";

const char kSuggestionsBlacklistURLParam[] = "url";
const char kSuggestionsDeviceParam[] = "t=%s";
const char kSuggestionsMinParam[] = "num=%i";

const char kSuggestionsMinVariationName[] = "min_suggestions";
const int kSuggestionsMinVariationDefault = 0;

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

int GetMinimumSuggestionsCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kUseSuggestionsEvenIfFewFeature, kSuggestionsMinVariationName,
      kSuggestionsMinVariationDefault);
}

}  // namespace

SuggestionsServiceImpl::SuggestionsServiceImpl(
    identity::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    net::URLRequestContextGetter* url_request_context,
    std::unique_ptr<SuggestionsStore> suggestions_store,
    std::unique_ptr<ImageManager> thumbnail_manager,
    std::unique_ptr<BlacklistStore> blacklist_store,
    const base::TickClock* tick_clock)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      sync_service_observer_(this),
      history_sync_state_(syncer::UploadState::INITIALIZING),
      url_request_context_(url_request_context),
      suggestions_store_(std::move(suggestions_store)),
      thumbnail_manager_(std::move(thumbnail_manager)),
      blacklist_store_(std::move(blacklist_store)),
      tick_clock_(tick_clock),
      blacklist_upload_backoff_(&kBlacklistBackoffPolicy, tick_clock_),
      blacklist_upload_timer_(tick_clock_),
      weak_ptr_factory_(this) {
  // |sync_service_| is null if switches::kDisableSync is set (tests use that).
  if (sync_service_)
    sync_service_observer_.Add(sync_service_);
  // Immediately get the current sync state, so we'll flush the cache if
  // necessary.
  OnStateChanged(sync_service_);
  // This makes sure the initial delay is actually respected.
  blacklist_upload_backoff_.InformOfRequest(/*succeeded=*/true);
}

SuggestionsServiceImpl::~SuggestionsServiceImpl() = default;

bool SuggestionsServiceImpl::FetchSuggestionsData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If sync state allows, issue a network request to refresh the suggestions.
  if (history_sync_state_ != syncer::UploadState::ACTIVE)
    return false;
  IssueRequestIfNoneOngoing(BuildSuggestionsURL());
  return true;
}

base::Optional<SuggestionsProfile>
SuggestionsServiceImpl::GetSuggestionsDataFromCache() const {
  SuggestionsProfile suggestions;
  // In case of empty cache or error, return empty.
  if (!suggestions_store_->LoadSuggestions(&suggestions))
    return base::nullopt;
  thumbnail_manager_->Initialize(suggestions);
  blacklist_store_->FilterSuggestions(&suggestions);
  return suggestions;
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

  // TODO(treib): Do we need to check |history_sync_state_| here?

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

  // TODO(treib): Do we need to check |history_sync_state_| here?

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

  // TODO(treib): Do we need to check |history_sync_state_| here?

  blacklist_store_->ClearBlacklist();
  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));
  IssueRequestIfNoneOngoing(BuildSuggestionsBlacklistClearURL());
}

base::TimeDelta SuggestionsServiceImpl::BlacklistDelayForTesting() const {
  return blacklist_upload_backoff_.GetTimeUntilRelease();
}

bool SuggestionsServiceImpl::HasPendingRequestForTesting() const {
  return !!pending_request_.get();
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
  std::string device = base::StringPrintf(kSuggestionsDeviceParam, kDeviceType);
  std::string query = device;
  if (base::FeatureList::IsEnabled(kUseSuggestionsEvenIfFewFeature)) {
    std::string min_suggestions =
        base::StringPrintf(kSuggestionsMinParam, GetMinimumSuggestionsCount());
    query =
        base::StringPrintf("%s&%s", device.c_str(), min_suggestions.c_str());
  }
  return GURL(base::StringPrintf(
      kSuggestionsURLFormat, GetGoogleBaseURL().spec().c_str(), query.c_str()));
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

SuggestionsServiceImpl::RefreshAction
SuggestionsServiceImpl::RefreshHistorySyncState() {
  syncer::UploadState new_sync_state = syncer::GetUploadToGoogleState(
      sync_service_, syncer::HISTORY_DELETE_DIRECTIVES);
  if (history_sync_state_ == new_sync_state)
    return NO_ACTION;

  syncer::UploadState old_sync_state = history_sync_state_;
  history_sync_state_ = new_sync_state;

  switch (new_sync_state) {
    case syncer::UploadState::INITIALIZING:
      // In this state, we do not issue server requests, but we will serve from
      // cache if available -> no action required.
      return NO_ACTION;
    case syncer::UploadState::ACTIVE:
      // If history sync was just enabled, immediately fetch suggestions, so
      // that hopefully the next NTP will already get them.
      if (old_sync_state == syncer::UploadState::NOT_ACTIVE)
        return FETCH_SUGGESTIONS;
      // Otherwise, this just means sync initialization finished.
      return NO_ACTION;
    case syncer::UploadState::NOT_ACTIVE:
      // If the user signed out (or disabled history sync), we have to clear
      // everything.
      return CLEAR_SUGGESTIONS;
  }
  NOTREACHED();
  return NO_ACTION;
}

void SuggestionsServiceImpl::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(sync_service_ == sync);

  switch (RefreshHistorySyncState()) {
    case NO_ACTION:
      break;
    case CLEAR_SUGGESTIONS:
      // Cancel any ongoing request, to stop interacting with the server.
      pending_request_.reset(nullptr);
      suggestions_store_->ClearSuggestions();
      callback_list_.Notify(SuggestionsProfile());
      break;
    case FETCH_SUGGESTIONS:
      IssueRequestIfNoneOngoing(BuildSuggestionsURL());
      break;
  }
}

void SuggestionsServiceImpl::SetDefaultExpiryTimestamp(
    SuggestionsProfile* suggestions,
    int64_t default_timestamp_usec) {
  for (ChromeSuggestion& suggestion : *suggestions->mutable_suggestions()) {
    // Do not set expiry if the server has already provided a more specific
    // expiry time for this suggestion.
    if (!suggestion.has_expiry_ts())
      suggestion.set_expiry_ts(default_timestamp_usec);
  }
}

void SuggestionsServiceImpl::IssueRequestIfNoneOngoing(const GURL& url) {
  // If there is an ongoing request, let it complete.
  // This will silently swallow blacklist and clearblacklist requests if a
  // request happens to be ongoing.
  // TODO(treib): Queue such requests and send them after the current one
  // completes.
  if (pending_request_.get())
    return;
  // If there is an ongoing token request, also wait for that.
  if (token_fetcher_)
    return;

  OAuth2TokenService::ScopeSet scopes{GaiaConstants::kChromeSyncOAuth2Scope};
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForPrimaryAccount(
      "suggestions_service", scopes,
      base::BindOnce(&SuggestionsServiceImpl::AccessTokenAvailable,
                     base::Unretained(this), url),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void SuggestionsServiceImpl::AccessTokenAvailable(
    const GURL& url,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK(token_fetcher_);
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      token_fetcher_deleter(std::move(token_fetcher_));

  if (error.state() != GoogleServiceAuthError::NONE) {
    blacklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
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
  last_request_started_time_ = tick_clock_->NowTicks();
}

std::unique_ptr<net::URLFetcher>
SuggestionsServiceImpl::CreateSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("suggestions_service", R"(
        semantics {
          sender: "Suggestions Service"
          description:
            "For signed-in users with History Sync enabled, the Suggestions "
            "Service fetches website suggestions, based on the user's browsing "
            "history, for display on the New Tab page."
          trigger: "Opening a New Tab page."
          data: "The user's OAuth2 credentials."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this feature by signing out of Chromium, or "
            "disabling Sync or History Sync in Chromium settings under "
            "Advanced sync settings. The feature is enabled by default."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  std::unique_ptr<net::URLFetcher> request = net::URLFetcher::Create(
      0, url, net::URLFetcher::GET, this, traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      request.get(), data_use_measurement::DataUseUserData::SUGGESTIONS);
  int load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_DO_NOT_SEND_COOKIES |
                   net::LOAD_DO_NOT_SAVE_COOKIES;

  request->SetLoadFlags(load_flags);
  request->SetRequestContext(url_request_context_);
  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // Note: It's OK to pass SignedIn::kNo if it's unknown, as it does not affect
  // transmission of experiments coming from the variations server.
  variations::AppendVariationHeaders(request->GetOriginalURL(),
                                     variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);
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
    base::UmaHistogramSparse("Suggestions.FailedRequestErrorCode",
                             -request_status.error());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    blacklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleBlacklistUpload();
    return;
  }

  const int response_code = request->GetResponseCode();
  base::UmaHistogramSparse("Suggestions.FetchResponseCode", response_code);
  if (response_code != net::HTTP_OK) {
    // A non-200 response code means that server has no (longer) suggestions for
    // this user. Aggressively clear the cache.
    suggestions_store_->ClearSuggestions();
    blacklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleBlacklistUpload();
    return;
  }

  const TimeDelta latency =
      tick_clock_->NowTicks() - last_request_started_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Suggestions.FetchSuccessLatency", latency);

  // Handle a successful blacklisting.
  GURL blacklisted_url;
  if (GetBlacklistedUrl(*source, &blacklisted_url))
    blacklist_store_->RemoveUrl(blacklisted_url);

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

  blacklist_upload_backoff_.InformOfRequest(/*succeeded=*/true);
  ScheduleBlacklistUpload();
}

void SuggestionsServiceImpl::PopulateExtraData(
    SuggestionsProfile* suggestions) {
  for (ChromeSuggestion& suggestion : *suggestions->mutable_suggestions()) {
    if (!suggestion.has_favicon_url() || suggestion.favicon_url().empty()) {
      suggestion.set_favicon_url(
          base::StringPrintf(kFaviconURL, suggestion.url().c_str()));
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
    blacklist_upload_timer_.Start(
        FROM_HERE, time_delta + blacklist_upload_backoff_.GetTimeUntilRelease(),
        base::Bind(&SuggestionsServiceImpl::UploadOneFromBlacklist,
                   weak_ptr_factory_.GetWeakPtr()));
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

}  // namespace suggestions
