// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_service.h"

#include <set>

#include "base/build_time.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/generated_resources_map.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_seed_simulator.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
#include "chrome/browser/upgrade_detector_impl.h"
#endif

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

// Gets the version number to use for variations seed simulation. Must be called
// on a thread where IO is allowed.
base::Version GetVersionForSimulation() {
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
  const base::Version installed_version =
      UpgradeDetectorImpl::GetCurrentlyInstalledVersion();
  if (installed_version.IsValid())
    return installed_version;
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)

  // TODO(asvitkine): Get the version that will be used on restart instead of
  // the current version on Android, iOS and ChromeOS.
  return base::Version(chrome::VersionInfo().Version());
}

// Gets the restrict parameter from |policy_pref_service| or from Chrome OS
// settings in the case of that platform.
std::string GetRestrictParameterPref(PrefService* policy_pref_service) {
  std::string parameter;
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetString(
      chromeos::kVariationsRestrictParameter, &parameter);
#else
  if (policy_pref_service) {
    parameter =
        policy_pref_service->GetString(prefs::kVariationsRestrictParameter);
  }
#endif
  return parameter;
}

enum ResourceRequestsAllowedState {
  RESOURCE_REQUESTS_ALLOWED,
  RESOURCE_REQUESTS_NOT_ALLOWED,
  RESOURCE_REQUESTS_ALLOWED_NOTIFIED,
  RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED,
  RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN,
  RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED,
  RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE,
};

// Records UMA histogram with the current resource requests allowed state.
void RecordRequestsAllowedHistogram(ResourceRequestsAllowedState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.ResourceRequestsAllowed", state,
                            RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE);
}

// Converts ResourceRequestAllowedNotifier::State to the corresponding
// ResourceRequestsAllowedState value.
ResourceRequestsAllowedState ResourceRequestStateToHistogramValue(
    ResourceRequestAllowedNotifier::State state) {
  switch (state) {
    case ResourceRequestAllowedNotifier::DISALLOWED_EULA_NOT_ACCEPTED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED;
    case ResourceRequestAllowedNotifier::DISALLOWED_NETWORK_DOWN:
      return RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN;
    case ResourceRequestAllowedNotifier::DISALLOWED_COMMAND_LINE_DISABLED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED;
    case ResourceRequestAllowedNotifier::ALLOWED:
      return RESOURCE_REQUESTS_ALLOWED;
  }
  NOTREACHED();
  return RESOURCE_REQUESTS_NOT_ALLOWED;
}


// Gets current form factor and converts it from enum DeviceFormFactor to enum
// Study_FormFactor.
Study_FormFactor GetCurrentFormFactor() {
  switch (ui::GetDeviceFormFactor()) {
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return Study_FormFactor_PHONE;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return Study_FormFactor_TABLET;
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return Study_FormFactor_DESKTOP;
  }
  NOTREACHED();
  return Study_FormFactor_DESKTOP;
}

// Gets the hardware class and returns it as a string. This returns an empty
// string if the client is not ChromeOS.
std::string GetHardwareClass() {
#if defined(OS_CHROMEOS)
  return base::SysInfo::GetLsbReleaseBoard();
#endif  // OS_CHROMEOS
  return std::string();
}

// Returns the date that should be used by the VariationsSeedProcessor to do
// expiry and start date checks.
base::Time GetReferenceDateForExpiryChecks(PrefService* local_state) {
  const int64 date_value = local_state->GetInt64(prefs::kVariationsSeedDate);
  const base::Time seed_date = base::Time::FromInternalValue(date_value);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;
  return reference_date;
}

// Overrides the string resource sepecified by |hash| with |string| in the
// resource bundle. Used as a callback passed to the variations seed processor.
void OverrideUIString(uint32_t hash, const base::string16& string) {
  int resource_id = GetResourceIndex(hash);
  if (resource_id == -1)
    return;

  ui::ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      resource_id, string);
}

}  // namespace

VariationsService::VariationsService(
    ResourceRequestAllowedNotifier* notifier,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager)
    : local_state_(local_state),
      state_manager_(state_manager),
      policy_pref_service_(local_state),
      seed_store_(local_state),
      create_trials_from_seed_called_(false),
      initial_request_completed_(false),
      resource_request_allowed_notifier_(notifier),
      weak_ptr_factory_(this) {
  resource_request_allowed_notifier_->Init(this);
}

VariationsService::~VariationsService() {
}

bool VariationsService::CreateTrialsFromSeed() {
  create_trials_from_seed_called_ = true;

  VariationsSeed seed;
  if (!seed_store_.LoadSeed(&seed))
    return false;

  const chrome::VersionInfo current_version_info;
  if (!current_version_info.is_valid())
    return false;

  const base::Version current_version(current_version_info.Version());
  if (!current_version.IsValid())
    return false;

  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed,
      g_browser_process->GetApplicationLocale(),
      GetReferenceDateForExpiryChecks(local_state_),
      current_version,
      GetChannelForVariations(),
      GetCurrentFormFactor(),
      GetHardwareClass(),
      base::Bind(&OverrideUIString));

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

  // Initialize the Variations server URL.
  variations_server_url_ = GetVariationsServerURL(policy_pref_service_);

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

void VariationsService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VariationsService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// TODO(rkaplow): Handle this and the similar event in metrics_service by
// observing an 'OnAppEnterForeground' event in RequestScheduler instead of
// requiring the frontend code to notify each service individually. Since the
// scheduler will handle it directly the VariationService shouldn't need to
// know details of this anymore.
void VariationsService::OnAppEnterForeground() {
  request_scheduler_->OnAppEnterForeground();
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
GURL VariationsService::GetVariationsServerURL(
    PrefService* policy_pref_service) {
  std::string server_url_string(CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kVariationsServerURL));
  if (server_url_string.empty())
    server_url_string = kDefaultVariationsServerURL;
  GURL server_url = GURL(server_url_string);

  const std::string restrict_param =
      GetRestrictParameterPref(policy_pref_service);
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

// static
std::string VariationsService::GetDefaultVariationsServerURLForTesting() {
  return kDefaultVariationsServerURL;
}

// static
void VariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  VariationsSeedStore::RegisterPrefs(registry);
  registry->RegisterInt64Pref(prefs::kVariationsLastFetchTime, 0);
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
}

// static
void VariationsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(
      prefs::kVariationsRestrictParameter,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
scoped_ptr<VariationsService> VariationsService::Create(
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager) {
  scoped_ptr<VariationsService> result;
#if !defined(GOOGLE_CHROME_BUILD)
  // Unless the URL was provided, unsupported builds should return NULL to
  // indicate that the service should not be used.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVariationsServerURL)) {
    DVLOG(1) << "Not creating VariationsService in unofficial build without --"
             << switches::kVariationsServerURL << " specified.";
    return result.Pass();
  }
#endif
  result.reset(new VariationsService(
      new ResourceRequestAllowedNotifier, local_state, state_manager));
  return result.Pass();
}

void VariationsService::DoActualFetch() {
  pending_seed_request_.reset(net::URLFetcher::Create(
      0, variations_server_url_, net::URLFetcher::GET, this));
  pending_seed_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
  pending_seed_request_->SetRequestContext(
      g_browser_process->system_request_context());
  pending_seed_request_->SetMaxRetriesOn5xx(kMaxRetrySeedFetch);
  if (!seed_store_.variations_serial_number().empty()) {
    pending_seed_request_->AddExtraRequestHeader(
        "If-Match:" + seed_store_.variations_serial_number());
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

void VariationsService::StoreSeed(const std::string& seed_data,
                                  const std::string& seed_signature,
                                  const base::Time& date_fetched) {
  scoped_ptr<VariationsSeed> seed(new VariationsSeed);
  if (!seed_store_.StoreSeedData(seed_data, seed_signature, date_fetched,
                                 seed.get())) {
    return;
  }
  RecordLastFetchTime();

  // Perform seed simulation only if |state_manager_| is not-NULL. The state
  // manager may be NULL for some unit tests.
  if (!state_manager_)
    return;

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&GetVersionForSimulation),
      base::Bind(&VariationsService::PerformSimulationWithVersion,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&seed)));
}

void VariationsService::FetchVariationsSeed() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const ResourceRequestAllowedNotifier::State state =
      resource_request_allowed_notifier_->GetResourceRequestsAllowedState();
  RecordRequestsAllowedHistogram(ResourceRequestStateToHistogramValue(state));
  if (state != ResourceRequestAllowedNotifier::ALLOWED) {
    DVLOG(1) << "Resource requests were not allowed. Waiting for notification.";
    return;
  }

  DoActualFetch();
}

void VariationsService::NotifyObservers(
    const VariationsSeedSimulator::Result& result) {
  if (result.kill_critical_group_change_count > 0) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnExperimentChangesDetected(Observer::CRITICAL));
  } else if (result.kill_best_effort_group_change_count > 0) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnExperimentChangesDetected(Observer::BEST_EFFORT));
  }
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
      g_browser_process->network_time_tracker()->UpdateNetworkTime(
          response_date,
          base::TimeDelta::FromMilliseconds(kServerTimeResolutionMs),
          latency,
          base::TimeTicks::Now());
    }
  }

  if (response_code != net::HTTP_OK) {
    DVLOG(1) << "Variations server request returned non-HTTP_OK response code: "
             << response_code;
    if (response_code == net::HTTP_NOT_MODIFIED) {
      RecordLastFetchTime();
      // Update the seed date value in local state (used for expiry check on
      // next start up), since 304 is a successful response.
      seed_store_.UpdateSeedDateAndLogDayChange(response_date);
    }
    return;
  }

  std::string seed_data;
  bool success = request->GetResponseAsString(&seed_data);
  DCHECK(success);

  std::string seed_signature;
  request->GetResponseHeaders()->EnumerateHeader(NULL,
                                                 "X-Seed-Signature",
                                                 &seed_signature);
  StoreSeed(seed_data, seed_signature, response_date);
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

void VariationsService::PerformSimulationWithVersion(
    scoped_ptr<VariationsSeed> seed,
    const base::Version& version) {
  if (version.IsValid())
    return;

  const base::ElapsedTimer timer;

  scoped_ptr<const base::FieldTrial::EntropyProvider> entropy_provider =
      state_manager_->CreateEntropyProvider();
  VariationsSeedSimulator seed_simulator(*entropy_provider);

  VariationsSeedSimulator::Result result = seed_simulator.SimulateSeedStudies(
      *seed, g_browser_process->GetApplicationLocale(),
      GetReferenceDateForExpiryChecks(local_state_), version,
      GetChannelForVariations(), GetCurrentFormFactor(), GetHardwareClass());

  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.NormalChanges",
                           result.normal_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillBestEffortChanges",
                           result.kill_best_effort_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillCriticalChanges",
                           result.kill_critical_group_change_count);

  UMA_HISTOGRAM_TIMES("Variations.SimulateSeed.Duration", timer.Elapsed());

  NotifyObservers(result);
}

void VariationsService::RecordLastFetchTime() {
  // local_state_ is NULL in tests, so check it first.
  if (local_state_) {
    local_state_->SetInt64(prefs::kVariationsLastFetchTime,
                           base::Time::Now().ToInternalValue());
  }
}

}  // namespace chrome_variations
