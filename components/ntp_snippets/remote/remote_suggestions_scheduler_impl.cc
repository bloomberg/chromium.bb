// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"

#include <random>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/network_change_notifier.h"

namespace ntp_snippets {

namespace {

// The FetchingInterval enum specifies overlapping time intervals that are used
// for scheduling the next remote suggestion fetch. Therefore a timer is created
// for each interval. Initially all the timers are started at the same time.
// A fetch for a given interval is only performed at the moment (after the
// interval has elapsed) when the combination of two conditions associated with
// the interval is met.
// 1. Trigger contidion:
//   - STARTUP_*: the user starts / switches to Chrome;
//   - SHOWN_*: the suggestions surface is shown to the user;
//   - PERSISTENT_*: the OS initiates the scheduled fetching task (which has
//   been scheduled according to this interval).
// 2. Connectivity condition:
//   - *_WIFI: the user is on a connection that the OS classifies as unmetered;
//   - *_FALLBACK: holds for any functional internet connection.
//
// If a fetch failed, then only the corresponding timer is reset. The other
// timers are not touched.
enum class FetchingInterval {
  PERSISTENT_FALLBACK,
  PERSISTENT_WIFI,
  STARTUP_FALLBACK,
  STARTUP_WIFI,
  SHOWN_FALLBACK,
  SHOWN_WIFI,
  COUNT
};

// The following arrays specify default values for remote suggestions fetch
// intervals corresponding to individual user classes. The user classes are
// defined by the user classifier. There must be an array for each user class.
// The values of each array specify a default time interval for the intervals
// defined by the enum FetchingInterval. The default time intervals defined in
// the arrays can be overridden using different variation parameters.
const double kDefaultFetchingIntervalHoursRareNtpUser[] = {192.0, 96.0, 48.0,
                                                           24.0,  12.0, 8.0};
const double kDefaultFetchingIntervalHoursActiveNtpUser[] = {96.0, 48.0, 48.0,
                                                             24.0, 12.0, 8.0};
const double kDefaultFetchingIntervalHoursActiveSuggestionsConsumer[] = {
    48.0, 24.0, 24.0, 6.0, 2.0, 1.0};

// Variation parameters than can be used to override the default fetching
// intervals. For backwards compatibility, we do not rename
//  - the "fetching_" params (should be "persistent_fetching_");
//  - the "soft_" params (should be "shown_").
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user",
    "startup_fetching_interval_hours-fallback-rare_ntp_user",
    "startup_fetching_interval_hours-wifi-rare_ntp_user",
    "soft_fetching_interval_hours-fallback-rare_ntp_user",
    "soft_fetching_interval_hours-wifi-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user",
    "startup_fetching_interval_hours-fallback-active_ntp_user",
    "startup_fetching_interval_hours-wifi-active_ntp_user",
    "soft_fetching_interval_hours-fallback-active_ntp_user",
    "soft_fetching_interval_hours-wifi-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer",
    "startup_fetching_interval_hours-fallback-active_suggestions_consumer",
    "startup_fetching_interval_hours-wifi-active_suggestions_consumer",
    "soft_fetching_interval_hours-fallback-active_suggestions_consumer",
    "soft_fetching_interval_hours-wifi-active_suggestions_consumer"};

static_assert(
    static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalHoursRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalHoursActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalHoursActiveSuggestionsConsumer) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameActiveSuggestionsConsumer),
    "Fill in all the info for fetching intervals.");

const char* kTriggerTypeNames[] = {"persistent_scheduler_wake_up", "ntp_opened",
                                   "browser_foregrounded",
                                   "browser_cold_start"};

const char* kTriggerTypesParamName = "scheduler_trigger_types";
const char* kTriggerTypesParamValueForEmptyList = "-";

const int kBlockBackgroundFetchesMinutesAfterClearingHistory = 30;

const char kSnippetSoftFetchingIntervalOnUsageEventDeprecated[] =
    "ntp_snippets.soft_fetching_interval_on_usage_event";
const char kSnippetSoftFetchingIntervalOnNtpOpenedDeprecated[] =
    "ntp_snippets.soft_fetching_interval_on_ntp_opened";

// Returns the time interval to use for scheduling remote suggestion fetches for
// the given interval and user_class.
base::TimeDelta GetDesiredFetchingInterval(
    FetchingInterval interval,
    UserClassifier::UserClass user_class) {
  DCHECK(interval != FetchingInterval::COUNT);
  const unsigned int index = static_cast<unsigned int>(interval);
  DCHECK(index < arraysize(kDefaultFetchingIntervalHoursRareNtpUser));

  double default_value_hours = 0.0;
  const char* param_name = nullptr;
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      default_value_hours = kDefaultFetchingIntervalHoursRareNtpUser[index];
      param_name = kFetchingIntervalParamNameRareNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      default_value_hours = kDefaultFetchingIntervalHoursActiveNtpUser[index];
      param_name = kFetchingIntervalParamNameActiveNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      default_value_hours =
          kDefaultFetchingIntervalHoursActiveSuggestionsConsumer[index];
      param_name = kFetchingIntervalParamNameActiveSuggestionsConsumer[index];
      break;
  }

  double value_hours = base::GetFieldTrialParamByFeatureAsDouble(
      ntp_snippets::kArticleSuggestionsFeature, param_name,
      default_value_hours);

  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

void ReportTimeUntilFirstSoftTrigger(UserClassifier::UserClass user_class,
                                     base::TimeDelta time_until_first_trigger) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstSoftTrigger.RareNTPUser",
          time_until_first_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstSoftTrigger."
          "ActiveNTPUser",
          time_until_first_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilFirstSoftTrigger."
          "ActiveSuggestionsConsumer",
          time_until_first_trigger, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilShownFetch(UserClassifier::UserClass user_class,
                               base::TimeDelta time_until_shown_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "RareNTPUser",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "ActiveNTPUser",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilSoftFetch."
          "ActiveSuggestionsConsumer",
          time_until_shown_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilStartupFetch(UserClassifier::UserClass user_class,
                                 base::TimeDelta time_until_startup_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "RareNTPUser",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "ActiveNTPUser",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilStartupFetch."
          "ActiveSuggestionsConsumer",
          time_until_startup_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

void ReportTimeUntilPersistentFetch(
    UserClassifier::UserClass user_class,
    base::TimeDelta time_until_persistent_fetch) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "RareNTPUser",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "ActiveNTPUser",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NewTabPage.ContentSuggestions.TimeUntilPersistentFetch."
          "ActiveSuggestionsConsumer",
          time_until_persistent_fetch, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromDays(7),
          /*bucket_count=*/50);
      break;
  }
}

}  // namespace

class EulaState : public web_resource::EulaAcceptedNotifier::Observer {
 public:
  EulaState(PrefService* local_state_prefs,
            RemoteSuggestionsScheduler* scheduler)
      : eula_notifier_(
            web_resource::EulaAcceptedNotifier::Create(local_state_prefs)),
        scheduler_(scheduler) {
    // EulaNotifier is not constructed on some platforms (such as desktop).
    if (!eula_notifier_) {
      return;
    }

    // Register the observer.
    eula_notifier_->Init(this);
  }

  ~EulaState() = default;

  bool IsEulaAccepted() {
    if (!eula_notifier_) {
      return true;
    }
    return eula_notifier_->IsEulaAccepted();
  }

  // EulaAcceptedNotifier::Observer implementation.
  void OnEulaAccepted() override {
    // Emulate a persistent fetch - we really want to fetch, initially!
    // TODO(jkrcal): Find a cleaner solution. This is somewhat hacky and can
    // mess up with metrics.
    scheduler_->OnPersistentSchedulerWakeUp();
  }

 private:
  std::unique_ptr<web_resource::EulaAcceptedNotifier> eula_notifier_;
  RemoteSuggestionsScheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(EulaState);
};

// static
RemoteSuggestionsSchedulerImpl::FetchingSchedule
RemoteSuggestionsSchedulerImpl::FetchingSchedule::Empty() {
  return FetchingSchedule{base::TimeDelta(), base::TimeDelta(),
                          base::TimeDelta(), base::TimeDelta()};
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::operator==(
    const FetchingSchedule& other) const {
  return interval_persistent_wifi == other.interval_persistent_wifi &&
         interval_persistent_fallback == other.interval_persistent_fallback &&
         interval_startup_wifi == other.interval_startup_wifi &&
         interval_startup_fallback == other.interval_startup_fallback &&
         interval_shown_wifi == other.interval_shown_wifi &&
         interval_shown_fallback == other.interval_shown_fallback;
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::operator!=(
    const FetchingSchedule& other) const {
  return !operator==(other);
}

bool RemoteSuggestionsSchedulerImpl::FetchingSchedule::is_empty() const {
  return interval_persistent_wifi.is_zero() &&
         interval_persistent_fallback.is_zero() &&
         interval_startup_wifi.is_zero() &&
         interval_startup_fallback.is_zero() && interval_shown_wifi.is_zero() &&
         interval_shown_fallback.is_zero();
}

// The TriggerType enum specifies values for the events that can trigger
// fetching remote suggestions. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused. When adding new entries, also update the array
// |kTriggerTypeNames| above.
enum class RemoteSuggestionsSchedulerImpl::TriggerType {
  PERSISTENT_SCHEDULER_WAKE_UP = 0,
  NTP_OPENED = 1,
  BROWSER_FOREGROUNDED = 2,
  BROWSER_COLD_START = 3,
  COUNT
};

RemoteSuggestionsSchedulerImpl::RemoteSuggestionsSchedulerImpl(
    PersistentScheduler* persistent_scheduler,
    const UserClassifier* user_classifier,
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    std::unique_ptr<base::Clock> clock)
    : persistent_scheduler_(persistent_scheduler),
      provider_(nullptr),
      background_fetch_in_progress_(false),
      user_classifier_(user_classifier),
      request_throttler_rare_ntp_user_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER),
      request_throttler_active_ntp_user_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER),
      request_throttler_active_suggestions_consumer_(
          profile_prefs,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER),
      time_until_first_trigger_reported_(false),
      eula_state_(base::MakeUnique<EulaState>(local_state_prefs, this)),
      profile_prefs_(profile_prefs),
      clock_(std::move(clock)),
      enabled_triggers_(GetEnabledTriggerTypes()) {
  DCHECK(user_classifier);
  DCHECK(profile_prefs);

  // Cleanup procedure in M59. Remove for M62.
  profile_prefs_->ClearPref(kSnippetSoftFetchingIntervalOnUsageEventDeprecated);
  profile_prefs_->ClearPref(kSnippetSoftFetchingIntervalOnNtpOpenedDeprecated);

  LoadLastFetchingSchedule();
}

RemoteSuggestionsSchedulerImpl::~RemoteSuggestionsSchedulerImpl() = default;

// static
void RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSnippetPersistentFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetPersistentFetchingIntervalFallback,
                              0);
  registry->RegisterInt64Pref(prefs::kSnippetStartupFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetStartupFetchingIntervalFallback,
                              0);
  registry->RegisterInt64Pref(prefs::kSnippetShownFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetShownFetchingIntervalFallback, 0);
  registry->RegisterInt64Pref(prefs::kSnippetLastFetchAttempt, 0);

  // Cleanup procedure in M59. Remove for M62.
  registry->RegisterInt64Pref(
      kSnippetSoftFetchingIntervalOnUsageEventDeprecated, 0);
  registry->RegisterInt64Pref(kSnippetSoftFetchingIntervalOnNtpOpenedDeprecated,
                              0);
}

void RemoteSuggestionsSchedulerImpl::SetProvider(
    RemoteSuggestionsProvider* provider) {
  DCHECK(provider);
  provider_ = provider;
}

void RemoteSuggestionsSchedulerImpl::OnProviderActivated() {
  StartScheduling();
}

void RemoteSuggestionsSchedulerImpl::OnProviderDeactivated() {
  StopScheduling();
}

void RemoteSuggestionsSchedulerImpl::OnSuggestionsCleared() {
  // This should be called by |provider_| so it should exist.
  DCHECK(provider_);
  // Some user action causes suggestions to be cleared, fetch now (as an
  // interactive request).
  provider_->ReloadSuggestions();
}

void RemoteSuggestionsSchedulerImpl::OnHistoryCleared() {
  // Due to privacy, we should not fetch for a while (unless the user explicitly
  // asks for new suggestions) to give sync the time to propagate the changes in
  // history to the server.
  background_fetches_allowed_after_ =
      clock_->Now() + base::TimeDelta::FromMinutes(
                          kBlockBackgroundFetchesMinutesAfterClearingHistory);
  // After that time elapses, we should fetch as soon as possible.
  ClearLastFetchAttemptTime();
}

void RemoteSuggestionsSchedulerImpl::RescheduleFetching() {
  // Force the reschedule by stopping and starting it again.
  StopScheduling();
  StartScheduling();
}

bool RemoteSuggestionsSchedulerImpl::AcquireQuotaForInteractiveFetch() {
  return AcquireQuota(/*interactive_request=*/true);
}

void RemoteSuggestionsSchedulerImpl::OnInteractiveFetchFinished(
    Status fetch_status) {
  OnFetchCompleted(fetch_status);
}

void RemoteSuggestionsSchedulerImpl::OnPersistentSchedulerWakeUp() {
  RefetchInTheBackgroundIfAppropriate(
      TriggerType::PERSISTENT_SCHEDULER_WAKE_UP);
}

void RemoteSuggestionsSchedulerImpl::OnBrowserForegrounded() {
  // TODO(jkrcal): Consider that this is called whenever we open or return to an
  // Activity. Therefore, keep work light for fast start up calls.
  RefetchInTheBackgroundIfAppropriate(TriggerType::BROWSER_FOREGROUNDED);
}

void RemoteSuggestionsSchedulerImpl::OnBrowserColdStart() {
  // TODO(jkrcal): Consider that work here must be kept light for fast
  // cold start ups.
  RefetchInTheBackgroundIfAppropriate(TriggerType::BROWSER_COLD_START);
}

void RemoteSuggestionsSchedulerImpl::OnNTPOpened() {
  // TODO(jkrcal): Consider that this is called whenever we open an NTP.
  // Therefore, keep work light for fast start up calls.
  RefetchInTheBackgroundIfAppropriate(TriggerType::NTP_OPENED);
}

void RemoteSuggestionsSchedulerImpl::StartScheduling() {
  FetchingSchedule new_schedule = GetDesiredFetchingSchedule();

  if (schedule_ == new_schedule) {
    // Do not schedule if nothing has changed;
    return;
  }

  schedule_ = new_schedule;
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::StopScheduling() {
  if (schedule_.is_empty()) {
    // Do not unschedule if already switched off.
    return;
  }

  schedule_ = FetchingSchedule::Empty();
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::ApplyPersistentFetchingSchedule() {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (persistent_scheduler_) {
    if (schedule_.is_empty()) {
      persistent_scheduler_->Unschedule();
    } else {
      persistent_scheduler_->Schedule(schedule_.interval_persistent_wifi,
                                      schedule_.interval_persistent_fallback);
    }
  }
}

RemoteSuggestionsSchedulerImpl::FetchingSchedule
RemoteSuggestionsSchedulerImpl::GetDesiredFetchingSchedule() const {
  UserClassifier::UserClass user_class = user_classifier_->GetUserClass();

  FetchingSchedule schedule;
  schedule.interval_persistent_wifi =
      GetDesiredFetchingInterval(FetchingInterval::PERSISTENT_WIFI, user_class);
  schedule.interval_persistent_fallback = GetDesiredFetchingInterval(
      FetchingInterval::PERSISTENT_FALLBACK, user_class);
  schedule.interval_startup_wifi =
      GetDesiredFetchingInterval(FetchingInterval::STARTUP_WIFI, user_class);
  schedule.interval_startup_fallback = GetDesiredFetchingInterval(
      FetchingInterval::STARTUP_FALLBACK, user_class);
  schedule.interval_shown_wifi =
      GetDesiredFetchingInterval(FetchingInterval::SHOWN_WIFI, user_class);
  schedule.interval_shown_fallback =
      GetDesiredFetchingInterval(FetchingInterval::SHOWN_FALLBACK, user_class);

  return schedule;
}

void RemoteSuggestionsSchedulerImpl::LoadLastFetchingSchedule() {
  schedule_.interval_persistent_wifi = base::TimeDelta::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetPersistentFetchingIntervalWifi));
  schedule_.interval_persistent_fallback =
      base::TimeDelta::FromInternalValue(profile_prefs_->GetInt64(
          prefs::kSnippetPersistentFetchingIntervalFallback));
  schedule_.interval_startup_wifi = base::TimeDelta::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetStartupFetchingIntervalWifi));
  schedule_.interval_startup_fallback = base::TimeDelta::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetStartupFetchingIntervalFallback));
  schedule_.interval_shown_wifi = base::TimeDelta::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetShownFetchingIntervalWifi));
  schedule_.interval_shown_fallback = base::TimeDelta::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetShownFetchingIntervalFallback));
}

void RemoteSuggestionsSchedulerImpl::StoreFetchingSchedule() {
  profile_prefs_->SetInt64(
      prefs::kSnippetPersistentFetchingIntervalWifi,
      schedule_.interval_persistent_wifi.ToInternalValue());
  profile_prefs_->SetInt64(
      prefs::kSnippetPersistentFetchingIntervalFallback,
      schedule_.interval_persistent_fallback.ToInternalValue());
  profile_prefs_->SetInt64(prefs::kSnippetStartupFetchingIntervalWifi,
                           schedule_.interval_startup_wifi.ToInternalValue());
  profile_prefs_->SetInt64(
      prefs::kSnippetStartupFetchingIntervalFallback,
      schedule_.interval_startup_fallback.ToInternalValue());
  profile_prefs_->SetInt64(prefs::kSnippetShownFetchingIntervalWifi,
                           schedule_.interval_shown_wifi.ToInternalValue());
  profile_prefs_->SetInt64(prefs::kSnippetShownFetchingIntervalFallback,
                           schedule_.interval_shown_fallback.ToInternalValue());
}

void RemoteSuggestionsSchedulerImpl::RefetchInTheBackgroundIfAppropriate(
    TriggerType trigger) {
  if (background_fetch_in_progress_) {
    return;
  }

  if (BackgroundFetchesDisabled(trigger)) {
    return;
  }

  bool is_soft = trigger != TriggerType::PERSISTENT_SCHEDULER_WAKE_UP;
  const base::Time last_fetch_attempt_time = base::Time::FromInternalValue(
      profile_prefs_->GetInt64(prefs::kSnippetLastFetchAttempt));

  if (is_soft && !time_until_first_trigger_reported_) {
    time_until_first_trigger_reported_ = true;
    ReportTimeUntilFirstSoftTrigger(user_classifier_->GetUserClass(),
                                    clock_->Now() - last_fetch_attempt_time);
  }

  if (is_soft &&
      !ShouldRefetchInTheBackgroundNow(last_fetch_attempt_time, trigger)) {
    return;
  }

  if (!AcquireQuota(/*interactive_request=*/false)) {
    return;
  }

  base::TimeDelta diff = clock_->Now() - last_fetch_attempt_time;
  switch (trigger) {
    case TriggerType::PERSISTENT_SCHEDULER_WAKE_UP:
      ReportTimeUntilPersistentFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::NTP_OPENED:
      ReportTimeUntilShownFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::BROWSER_FOREGROUNDED:
    case TriggerType::BROWSER_COLD_START:
      ReportTimeUntilStartupFetch(user_classifier_->GetUserClass(), diff);
      break;
    case TriggerType::COUNT:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.ContentSuggestions.BackgroundFetchTrigger",
      static_cast<int>(trigger), static_cast<int>(TriggerType::COUNT));

  background_fetch_in_progress_ = true;
  provider_->RefetchInTheBackground(base::Bind(
      &RemoteSuggestionsSchedulerImpl::RefetchInTheBackgroundFinished,
      base::Unretained(this)));
}

bool RemoteSuggestionsSchedulerImpl::ShouldRefetchInTheBackgroundNow(
    base::Time last_fetch_attempt_time,
    TriggerType trigger) {
  // If we have no persistent scheduler to ask, err on the side of caution.
  bool wifi = false;
  if (persistent_scheduler_) {
    wifi = persistent_scheduler_->IsOnUnmeteredConnection();
  }

  base::Time first_allowed_fetch_time = last_fetch_attempt_time;
  if (trigger == TriggerType::NTP_OPENED) {
    first_allowed_fetch_time += (wifi ? schedule_.interval_shown_wifi
                                      : schedule_.interval_shown_fallback);
  } else {
    first_allowed_fetch_time += (wifi ? schedule_.interval_startup_wifi
                                      : schedule_.interval_startup_fallback);
  }

  base::Time now = clock_->Now();
  return background_fetches_allowed_after_ <= now &&
         first_allowed_fetch_time <= now;
}

bool RemoteSuggestionsSchedulerImpl::BackgroundFetchesDisabled(
    TriggerType trigger) const {
  if (!provider_) {
    return true;  // Cannot fetch as remote suggestions provider does not exist.
  }

  if (schedule_.is_empty()) {
    return true;  // Background fetches are disabled in general.
  }

  if (enabled_triggers_.count(trigger) == 0) {
    return true;  // Background fetches for |trigger| are not enabled.
  }

  if (!eula_state_->IsEulaAccepted()) {
    return true;  // No background fetches are allowed before EULA is accepted.
  }

  return false;
}

bool RemoteSuggestionsSchedulerImpl::AcquireQuota(bool interactive_request) {
  switch (user_classifier_->GetUserClass()) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return request_throttler_rare_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return request_throttler_active_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return request_throttler_active_suggestions_consumer_
          .DemandQuotaForRequest(interactive_request);
  }
  NOTREACHED();
  return false;
}

void RemoteSuggestionsSchedulerImpl::RefetchInTheBackgroundFinished(
    Status fetch_status) {
  background_fetch_in_progress_ = false;
  OnFetchCompleted(fetch_status);
}

void RemoteSuggestionsSchedulerImpl::OnFetchCompleted(Status fetch_status) {
  profile_prefs_->SetInt64(prefs::kSnippetLastFetchAttempt,
                           clock_->Now().ToInternalValue());
  time_until_first_trigger_reported_ = false;

  // Reschedule after a fetch. The persistent schedule is applied only after a
  // successful fetch. After a failed fetch, we want to keep the previous
  // persistent schedule intact so that we eventually get a persistent
  // fallback fetch (if the wifi persistent fetches keep failing).
  if (fetch_status.code != StatusCode::SUCCESS) {
    return;
  }
  ApplyPersistentFetchingSchedule();
}

void RemoteSuggestionsSchedulerImpl::ClearLastFetchAttemptTime() {
  profile_prefs_->ClearPref(prefs::kSnippetLastFetchAttempt);
}

std::set<RemoteSuggestionsSchedulerImpl::TriggerType>
RemoteSuggestionsSchedulerImpl::GetEnabledTriggerTypes() {
  static_assert(static_cast<unsigned int>(TriggerType::COUNT) ==
                    arraysize(kTriggerTypeNames),
                "Fill in names for trigger types.");

  std::string param_value = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, kTriggerTypesParamName);
  if (param_value == kTriggerTypesParamValueForEmptyList) {
    return std::set<TriggerType>();
  }

  std::vector<std::string> tokens = base::SplitString(
      param_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) {
    return GetDefaultEnabledTriggerTypes();
  }

  std::set<TriggerType> enabled_types;
  for (const auto& token : tokens) {
    auto** it = std::find(std::begin(kTriggerTypeNames),
                          std::end(kTriggerTypeNames), token);
    if (it == std::end(kTriggerTypeNames)) {
      DLOG(WARNING) << "Failed to parse variation param "
                    << kTriggerTypesParamName << " with string value "
                    << param_value
                    << " into a comma-separated list of keywords. "
                    << "Unknown token " << token
                    << " found. Falling back to default value.";
      return GetDefaultEnabledTriggerTypes();
    }

    // Add the enabled type represented by |token| into the result set.
    enabled_types.insert(
        static_cast<TriggerType>(it - std::begin(kTriggerTypeNames)));
  }
  return enabled_types;
}

std::set<RemoteSuggestionsSchedulerImpl::TriggerType>
RemoteSuggestionsSchedulerImpl::GetDefaultEnabledTriggerTypes() {
  return {TriggerType::PERSISTENT_SCHEDULER_WAKE_UP, TriggerType::NTP_OPENED,
          TriggerType::BROWSER_COLD_START, TriggerType::BROWSER_FOREGROUNDED};
}

}  // namespace ntp_snippets
