// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_service.h"

#include <set>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/network_time/network_time_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_processor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace chrome_variations {

namespace {

// Default server of Variations seed info.
const char kDefaultVariationsServerURL[] =
    "https://clients4.google.com/chrome-variations/seed";
const int kMaxRetrySeedFetch = 5;

// TODO(mad): To be removed when we stop updating the NetworkTimeTracker.
// For the HTTP date headers, the resolution of the server time is 1 second.
const int64 kServerTimeResolutionMs = 1000;

// Wrapper around channel checking, used to enable channel mocking for
// testing. If the current browser channel is not UNKNOWN, this will return
// that channel value. Otherwise, if the fake channel flag is provided, this
// will return the fake channel. Failing that, this will return the UNKNOWN
// channel.
Study_Channel GetChannelForVariations() {
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_CANARY:
      return Study_Channel_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return Study_Channel_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return Study_Channel_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return Study_Channel_STABLE;
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      break;
  }
  const std::string forced_channel =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (forced_channel == "stable")
    return Study_Channel_STABLE;
  if (forced_channel == "beta")
    return Study_Channel_BETA;
  if (forced_channel == "dev")
    return Study_Channel_DEV;
  if (forced_channel == "canary")
    return Study_Channel_CANARY;
  DVLOG(1) << "Invalid channel provided: " << forced_channel;
  return Study_Channel_UNKNOWN;
}

// Returns a string that will be used for the value of the 'osname' URL param
// to the variations server.
std::string GetPlatformString() {
#if defined(OS_WIN)
  return "win";
#elif defined(OS_IOS)
  return "ios";
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_ANDROID)
  return "android";
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return "linux";
#else
#error Unknown platform
#endif
}

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64 date_time) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(date_time);
}

// Gets the restrict parameter from |local_state| or from Chrome OS settings in
// the case of that platform.
std::string GetRestrictParameterPref(PrefService* local_state) {
  std::string parameter;
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetString(
      chromeos::kVariationsRestrictParameter, &parameter);
#else
  if (local_state)
    parameter = local_state->GetString(prefs::kVariationsRestrictParameter);
#endif
  return parameter;
}

// Computes a hash of the serialized variations seed data.
std::string HashSeed(const std::string& seed_data) {
  const std::string sha1 = base::SHA1HashString(seed_data);
  return base::HexEncode(sha1.data(), sha1.size());
}

enum ResourceRequestsAllowedState {
  RESOURCE_REQUESTS_ALLOWED,
  RESOURCE_REQUESTS_NOT_ALLOWED,
  RESOURCE_REQUESTS_ALLOWED_NOTIFIED,
  RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE,
};

// Records UMA histogram with the current resource requests allowed state.
void RecordRequestsAllowedHistogram(ResourceRequestsAllowedState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.ResourceRequestsAllowed", state,
                            RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE);
}

enum VariationSeedEmptyState {
  VARIATIONS_SEED_NOT_EMPTY,
  VARIATIONS_SEED_EMPTY,
  VARIATIONS_SEED_CORRUPT,
  VARIATIONS_SEED_EMPTY_ENUM_SIZE,
};

void RecordVariationSeedEmptyHistogram(VariationSeedEmptyState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedEmpty", state,
                            VARIATIONS_SEED_EMPTY_ENUM_SIZE);
}

}  // namespace

VariationsService::VariationsService(PrefService* local_state)
    : local_state_(local_state),
      variations_server_url_(GetVariationsServerURL(local_state)),
      create_trials_from_seed_called_(false),
      initial_request_completed_(false),
      resource_request_allowed_notifier_(
          new ResourceRequestAllowedNotifier) {
  resource_request_allowed_notifier_->Init(this);
}

VariationsService::VariationsService(ResourceRequestAllowedNotifier* notifier,
                                     PrefService* local_state)
    : local_state_(local_state),
      variations_server_url_(GetVariationsServerURL(NULL)),
      create_trials_from_seed_called_(false),
      initial_request_completed_(false),
      resource_request_allowed_notifier_(notifier) {
  resource_request_allowed_notifier_->Init(this);
}

VariationsService::~VariationsService() {
}

bool VariationsService::CreateTrialsFromSeed() {
  create_trials_from_seed_called_ = true;

  VariationsSeed seed;
  if (!LoadVariationsSeedFromPref(&seed))
    return false;

  const int64 date_value = local_state_->GetInt64(prefs::kVariationsSeedDate);
  const base::Time seed_date = base::Time::FromInternalValue(date_value);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;

  const chrome::VersionInfo current_version_info;
  if (!current_version_info.is_valid())
    return false;

  const base::Version current_version(current_version_info.Version());
  if (!current_version.IsValid())
    return false;

  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, g_browser_process->GetApplicationLocale(), reference_date,
      current_version, GetChannelForVariations());

  // Log the "freshness" of the seed that was just used. The freshness is the
  // time between the last successful seed download and now.
  const int64 last_fetch_time_internal =
      local_state_->GetInt64(prefs::kVariationsLastFetchTime);
  if (last_fetch_time_internal) {
    const base::Time now = base::Time::Now();
    const base::TimeDelta delta =
        now - base::Time::FromInternalValue(last_fetch_time_internal);
    // Log the value in number of minutes.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", delta.InMinutes(),
        1, base::TimeDelta::FromDays(30).InMinutes(), 50);
  }

  return true;
}

void VariationsService::StartRepeatedVariationsSeedFetch() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check that |CreateTrialsFromSeed| was called, which is necessary to
  // retrieve the serial number that will be sent to the server.
  DCHECK(create_trials_from_seed_called_);

  DCHECK(!request_scheduler_.get());
  // Note that the act of instantiating the scheduler will start the fetch, if
  // the scheduler deems appropriate. Using Unretained is fine here since the
  // lifespan of request_scheduler_ is guaranteed to be shorter than that of
  // this service.
  request_scheduler_.reset(VariationsRequestScheduler::Create(
      base::Bind(&VariationsService::FetchVariationsSeed,
          base::Unretained(this)), local_state_));
  request_scheduler_->Start();
}

// static
GURL VariationsService::GetVariationsServerURL(PrefService* local_state) {
  std::string server_url_string(CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kVariationsServerURL));
  if (server_url_string.empty())
    server_url_string = kDefaultVariationsServerURL;
  GURL server_url = GURL(server_url_string);

  const std::string restrict_param = GetRestrictParameterPref(local_state);
  if (!restrict_param.empty()) {
    server_url = net::AppendOrReplaceQueryParameter(server_url,
                                                    "restrict",
                                                    restrict_param);
  }

  server_url = net::AppendOrReplaceQueryParameter(server_url, "osname",
                                                  GetPlatformString());

  DCHECK(server_url.is_valid());
  return server_url;
}

#if defined(OS_WIN)
void VariationsService::StartGoogleUpdateRegistrySync() {
  registry_syncer_.RequestRegistrySync();
}
#endif

void VariationsService::SetCreateTrialsFromSeedCalledForTesting(bool called) {
  create_trials_from_seed_called_ = called;
}

// static
std::string VariationsService::GetDefaultVariationsServerURLForTesting() {
  return kDefaultVariationsServerURL;
}

// static
void VariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsSeedHash, std::string());
  registry->RegisterInt64Pref(prefs::kVariationsSeedDate,
                              base::Time().ToInternalValue());
  registry->RegisterInt64Pref(prefs::kVariationsLastFetchTime, 0);
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
}

// static
VariationsService* VariationsService::Create(PrefService* local_state) {
#if !defined(GOOGLE_CHROME_BUILD)
  // Unless the URL was provided, unsupported builds should return NULL to
  // indicate that the service should not be used.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVariationsServerURL)) {
    DVLOG(1) << "Not creating VariationsService in unofficial build without --"
             << switches::kVariationsServerURL << " specified.";
    return NULL;
  }
#endif
  return new VariationsService(local_state);
}

void VariationsService::DoActualFetch() {
  pending_seed_request_.reset(net::URLFetcher::Create(
      0, variations_server_url_, net::URLFetcher::GET, this));
  pending_seed_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
  pending_seed_request_->SetRequestContext(
      g_browser_process->system_request_context());
  pending_seed_request_->SetMaxRetriesOn5xx(kMaxRetrySeedFetch);
  if (!variations_serial_number_.empty()) {
    pending_seed_request_->AddExtraRequestHeader("If-Match:" +
                                                 variations_serial_number_);
  }
  pending_seed_request_->Start();

  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_fetch;
  // Record a time delta of 0 (default value) if there was no previous fetch.
  if (!last_request_started_time_.is_null())
    time_since_last_fetch = now - last_request_started_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.TimeSinceLastFetchAttempt",
                              time_since_last_fetch.InMinutes(), 0,
                              base::TimeDelta::FromDays(7).InMinutes(), 50);
  last_request_started_time_ = now;
}

void VariationsService::FetchVariationsSeed() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!resource_request_allowed_notifier_->ResourceRequestsAllowed()) {
    RecordRequestsAllowedHistogram(RESOURCE_REQUESTS_NOT_ALLOWED);
    DVLOG(1) << "Resource requests were not allowed. Waiting for notification.";
    return;
  }

  RecordRequestsAllowedHistogram(RESOURCE_REQUESTS_ALLOWED);
  DoActualFetch();
}

void VariationsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(pending_seed_request_.get(), source);

  const bool is_first_request = !initial_request_completed_;
  initial_request_completed_ = true;

  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(pending_seed_request_.release());
  const net::URLRequestStatus& request_status = request->GetStatus();
  if (request_status.status() != net::URLRequestStatus::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Variations.FailedRequestErrorCode",
                                -request_status.error());
    DVLOG(1) << "Variations server request failed with error: "
             << request_status.error() << ": "
             << net::ErrorToString(request_status.error());
    // It's common for the very first fetch attempt to fail (e.g. the network
    // may not yet be available). In such a case, try again soon, rather than
    // waiting the full time interval.
    if (is_first_request)
      request_scheduler_->ScheduleFetchShortly();
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY("Variations.SeedFetchResponseCode",
                              response_code);

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;

  base::Time response_date;
  if (response_code == net::HTTP_OK ||
      response_code == net::HTTP_NOT_MODIFIED) {
    bool success = request->GetResponseHeaders()->GetDateValue(&response_date);
    DCHECK(success || response_date.is_null());

    if (!response_date.is_null()) {
      NetworkTimeTracker::BuildNotifierUpdateCallback().Run(
          response_date,
          base::TimeDelta::FromMilliseconds(kServerTimeResolutionMs),
          latency);
    }
  }

  if (response_code != net::HTTP_OK) {
    DVLOG(1) << "Variations server request returned non-HTTP_OK response code: "
             << response_code;
    if (response_code == net::HTTP_NOT_MODIFIED) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchNotModifiedLatency", latency);
      RecordLastFetchTime();
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchOtherLatency", latency);
    }
    return;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchSuccessLatency", latency);

  std::string seed_data;
  bool success = request->GetResponseAsString(&seed_data);
  DCHECK(success);

  StoreSeedData(seed_data, response_date);
}

void VariationsService::OnResourceRequestsAllowed() {
  // Note that this only attempts to fetch the seed at most once per period
  // (kSeedFetchPeriodHours). This works because
  // |resource_request_allowed_notifier_| only calls this method if an
  // attempt was made earlier that fails (which implies that the period had
  // elapsed). After a successful attempt is made, the notifier will know not
  // to call this method again until another failed attempt occurs.
  RecordRequestsAllowedHistogram(RESOURCE_REQUESTS_ALLOWED_NOTIFIED);
  DVLOG(1) << "Retrying fetch.";
  DoActualFetch();

  // This service must have created a scheduler in order for this to be called.
  DCHECK(request_scheduler_.get());
  request_scheduler_->Reset();
}

bool VariationsService::StoreSeedData(const std::string& seed_data,
                                      const base::Time& seed_date) {
  if (seed_data.empty()) {
    VLOG(1) << "Variations Seed data from server is empty, rejecting the seed.";
    return false;
  }

  // Only store the seed data if it parses correctly.
  VariationsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    VLOG(1) << "Variations Seed data from server is not in valid proto format, "
            << "rejecting the seed.";
    return false;
  }

  std::string base64_seed_data;
  if (!base::Base64Encode(seed_data, &base64_seed_data)) {
    VLOG(1) << "Variations Seed data from server fails Base64Encode, rejecting "
            << "the seed.";
    return false;
  }

  local_state_->SetString(prefs::kVariationsSeed, base64_seed_data);
  local_state_->SetString(prefs::kVariationsSeedHash, HashSeed(seed_data));
  local_state_->SetInt64(prefs::kVariationsSeedDate,
                         seed_date.ToInternalValue());
  variations_serial_number_ = seed.serial_number();

  RecordLastFetchTime();

  return true;
}

bool VariationsService::LoadVariationsSeedFromPref(VariationsSeed* seed) {
  const std::string base64_seed_data =
      local_state_->GetString(prefs::kVariationsSeed);
  if (base64_seed_data.empty()) {
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_EMPTY);
    return false;
  }

  const std::string hash_from_pref =
      local_state_->GetString(prefs::kVariationsSeedHash);
  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string seed_data;
  if (!base::Base64Decode(base64_seed_data, &seed_data) ||
      (!hash_from_pref.empty() && HashSeed(seed_data) != hash_from_pref) ||
      !seed->ParseFromString(seed_data)) {
    VLOG(1) << "Variations seed data in local pref is corrupt, clearing the "
            << "pref.";
    local_state_->ClearPref(prefs::kVariationsSeed);
    local_state_->ClearPref(prefs::kVariationsSeedDate);
    local_state_->ClearPref(prefs::kVariationsSeedHash);
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT);
    return false;
  }
  variations_serial_number_ = seed->serial_number();
  RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_NOT_EMPTY);
  return true;
}

void VariationsService::RecordLastFetchTime() {
  // local_state_ is NULL in tests, so check it first.
  if (local_state_) {
    local_state_->SetInt64(prefs::kVariationsLastFetchTime,
                           base::Time::Now().ToInternalValue());
  }
}

}  // namespace chrome_variations
