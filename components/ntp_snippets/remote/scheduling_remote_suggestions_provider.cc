// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/scheduling_remote_suggestions_provider.h"

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
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {

namespace {

// The FetchingInterval enum specifies overlapping time intervals that are used
// for scheduling the next remote suggestion fetch. Therefore a timer is created
// for each interval. Initially all the timers are started at the same time.
// Fetches are
// only performed when certain conditions associated with the intervals are
// met. If a fetch failed, then only the corresponding timer is reset. The
// other timers are not touched.
// TODO(markusheintz): Describe the individual intervals.
enum class FetchingInterval {
  PERSISTENT_FALLBACK,
  PERSISTENT_WIFI,
  SOFT_ON_USAGE_EVENT,
  SOFT_ON_NTP_OPENED,
  COUNT
};

// The following arrays specify default values for remote suggestions fetch
// intervals corresponding to individual user classes. The user classes are
// defined by the user classifier. There must be an array for each user class.
// The values of each array specify a default time interval for the intervals
// defined by the enum FetchingInterval. The default time intervals defined in
// the arrays can be overridden using different variation parameters.
const double kDefaultFetchingIntervalHoursRareNtpUser[] = {48.0, 24.0, 12.0,
                                                           6.0};
const double kDefaultFetchingIntervalHoursActiveNtpUser[] = {24.0, 6.0, 2.0,
                                                             2.0};
const double kDefaultFetchingIntervalHoursActiveSuggestionsConsumer[] = {
    24.0, 6.0, 2.0, 1.0};

// Variation parameters than can be used to override the default fetching
// intervals.
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user",
    "soft_fetching_interval_hours-active-rare_ntp_user",
    "soft_on_ntp_opened_interval_hours-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user",
    "soft_fetching_interval_hours-active-active_ntp_user",
    "soft_on_ntp_opened_interval_hours-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer",
    "soft_fetching_interval_hours-active-active_suggestions_consumer",
    "soft_on_ntp_opened_interval_hours-active_suggestions_consumer"};

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

}  // namespace

// static
SchedulingRemoteSuggestionsProvider::FetchingSchedule
SchedulingRemoteSuggestionsProvider::FetchingSchedule::Empty() {
  return FetchingSchedule{base::TimeDelta(), base::TimeDelta(),
                          base::TimeDelta(), base::TimeDelta()};
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::operator==(
    const FetchingSchedule& other) const {
  return interval_persistent_wifi == other.interval_persistent_wifi &&
         interval_persistent_fallback == other.interval_persistent_fallback &&
         interval_soft_on_usage_event == other.interval_soft_on_usage_event &&
         interval_soft_on_ntp_opened == other.interval_soft_on_ntp_opened;
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::operator!=(
    const FetchingSchedule& other) const {
  return !operator==(other);
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::is_empty() const {
  return interval_persistent_wifi.is_zero() &&
         interval_persistent_fallback.is_zero() &&
         interval_soft_on_usage_event.is_zero() &&
         interval_soft_on_ntp_opened.is_zero();
}

// The TriggerType enum specifies values for the events that can trigger
// fetching remote suggestions. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused. When adding new entries, also update the array
// |kTriggerTypeNames| above.
enum class SchedulingRemoteSuggestionsProvider::TriggerType {
  PERSISTENT_SCHEDULER_WAKE_UP = 0,
  NTP_OPENED = 1,
  BROWSER_FOREGROUNDED = 2,
  BROWSER_COLD_START = 3,
  COUNT
};

SchedulingRemoteSuggestionsProvider::SchedulingRemoteSuggestionsProvider(
    Observer* observer,
    std::unique_ptr<RemoteSuggestionsProvider> provider,
    PersistentScheduler* persistent_scheduler,
    const UserClassifier* user_classifier,
    PrefService* pref_service,
    std::unique_ptr<base::Clock> clock)
    : RemoteSuggestionsProvider(observer),
      RemoteSuggestionsScheduler(),
      provider_(std::move(provider)),
      persistent_scheduler_(persistent_scheduler),
      background_fetch_in_progress_(false),
      user_classifier_(user_classifier),
      request_throttler_rare_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER),
      request_throttler_active_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER),
      request_throttler_active_suggestions_consumer_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER),
      pref_service_(pref_service),
      clock_(std::move(clock)),
      enabled_triggers_(GetEnabledTriggerTypes()) {
  DCHECK(user_classifier);
  DCHECK(pref_service);

  LoadLastFetchingSchedule();
}

SchedulingRemoteSuggestionsProvider::~SchedulingRemoteSuggestionsProvider() =
    default;

// static
void SchedulingRemoteSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSnippetPersistentFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetPersistentFetchingIntervalFallback,
                              0);
  registry->RegisterInt64Pref(prefs::kSnippetSoftFetchingIntervalOnUsageEvent,
                              0);
  registry->RegisterInt64Pref(prefs::kSnippetLastFetchAttempt, 0);
  registry->RegisterInt64Pref(prefs::kSnippetSoftFetchingIntervalOnNtpOpened,
                              0);
}

void SchedulingRemoteSuggestionsProvider::OnProviderActivated() {
  StartScheduling();
}

void SchedulingRemoteSuggestionsProvider::OnProviderDeactivated() {
  StopScheduling();
}

void SchedulingRemoteSuggestionsProvider::OnSuggestionsCleared() {
  // Some user action causes suggestions to be cleared, fetch now (as an
  // interactive request).
  ReloadSuggestions();
}

void SchedulingRemoteSuggestionsProvider::OnHistoryCleared() {
  // Due to privacy, we should not fetch for a while (unless the user explicitly
  // asks for new suggestions) to give sync the time to propagate the changes in
  // history to the server.
  background_fetches_allowed_after_ =
      clock_->Now() + base::TimeDelta::FromMinutes(
                          kBlockBackgroundFetchesMinutesAfterClearingHistory);
  // After that time elapses, we should fetch as soon as possible.
  ClearLastFetchAttemptTime();
}

void SchedulingRemoteSuggestionsProvider::RescheduleFetching() {
  // Force the reschedule by stopping and starting it again.
  StopScheduling();
  StartScheduling();
}

void SchedulingRemoteSuggestionsProvider::OnPersistentSchedulerWakeUp() {
  RefetchInTheBackgroundIfEnabled(TriggerType::PERSISTENT_SCHEDULER_WAKE_UP);
}

void SchedulingRemoteSuggestionsProvider::OnBrowserForegrounded() {
  // TODO(jkrcal): Consider that this is called whenever we open or return to an
  // Activity. Therefore, keep work light for fast start up calls.
  if (!ShouldRefetchInTheBackgroundNow(TriggerType::BROWSER_FOREGROUNDED)) {
    return;
  }

  RefetchInTheBackgroundIfEnabled(TriggerType::BROWSER_FOREGROUNDED);
}

void SchedulingRemoteSuggestionsProvider::OnBrowserColdStart() {
  // TODO(fhorschig|jkrcal): Consider that work here must be kept light for fast
  // cold start ups.
  if (!ShouldRefetchInTheBackgroundNow(TriggerType::BROWSER_COLD_START)) {
    return;
  }

  RefetchInTheBackgroundIfEnabled(TriggerType::BROWSER_COLD_START);
}

void SchedulingRemoteSuggestionsProvider::OnNTPOpened() {
  if (!ShouldRefetchInTheBackgroundNow(TriggerType::NTP_OPENED)) {
    return;
  }

  RefetchInTheBackgroundIfEnabled(TriggerType::NTP_OPENED);
}

void SchedulingRemoteSuggestionsProvider::RefetchInTheBackground(
    std::unique_ptr<FetchStatusCallback> callback) {
  if (background_fetch_in_progress_) {
    if (callback) {
      callback->Run(
          Status(StatusCode::TEMPORARY_ERROR, "Background fetch in progress"));
    }
    return;
  }

  if (!AcquireQuota(/*interactive_request=*/false)) {
    if (callback) {
      callback->Run(Status(StatusCode::TEMPORARY_ERROR,
                           "Non-interactive quota exceeded"));
    }
    return;
  }

  background_fetch_in_progress_ = true;
  RemoteSuggestionsProvider::FetchStatusCallback wrapper_callback = base::Bind(
      &SchedulingRemoteSuggestionsProvider::RefetchInTheBackgroundFinished,
      base::Unretained(this), base::Passed(&callback));
  provider_->RefetchInTheBackground(
      base::MakeUnique<RemoteSuggestionsProvider::FetchStatusCallback>(
          std::move(wrapper_callback)));
}

const RemoteSuggestionsFetcher*
SchedulingRemoteSuggestionsProvider::suggestions_fetcher_for_debugging() const {
  return provider_->suggestions_fetcher_for_debugging();
}

CategoryStatus SchedulingRemoteSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return provider_->GetCategoryStatus(category);
}

CategoryInfo SchedulingRemoteSuggestionsProvider::GetCategoryInfo(
    Category category) {
  return provider_->GetCategoryInfo(category);
}

void SchedulingRemoteSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  provider_->DismissSuggestion(suggestion_id);
}

void SchedulingRemoteSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  provider_->FetchSuggestionImage(suggestion_id, callback);
}

void SchedulingRemoteSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  if (!AcquireQuota(/*interactive_request=*/true)) {
    if (callback) {
      callback.Run(
          Status(StatusCode::TEMPORARY_ERROR, "Interactive quota exceeded"),
          std::vector<ContentSuggestion>());
    }
    return;
  }

  provider_->Fetch(
      category, known_suggestion_ids,
      base::Bind(&SchedulingRemoteSuggestionsProvider::FetchFinished,
                 base::Unretained(this), callback));
}

void SchedulingRemoteSuggestionsProvider::ReloadSuggestions() {
  if (!AcquireQuota(/*interactive_request=*/true)) {
    return;
  }

  provider_->ReloadSuggestions();
}

void SchedulingRemoteSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  provider_->ClearHistory(begin, end, filter);
}

void SchedulingRemoteSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  provider_->ClearCachedSuggestions(category);
}

void SchedulingRemoteSuggestionsProvider::OnSignInStateChanged() {
  provider_->OnSignInStateChanged();
}

void SchedulingRemoteSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  provider_->GetDismissedSuggestionsForDebugging(category, callback);
}

void SchedulingRemoteSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  provider_->ClearDismissedSuggestionsForDebugging(category);
}

void SchedulingRemoteSuggestionsProvider::StartScheduling() {
  FetchingSchedule new_schedule = GetDesiredFetchingSchedule();

  if (schedule_ == new_schedule) {
    // Do not schedule if nothing has changed;
    return;
  }

  schedule_ = new_schedule;
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void SchedulingRemoteSuggestionsProvider::StopScheduling() {
  if (schedule_.is_empty()) {
    // Do not unschedule if already switched off.
    return;
  }

  schedule_ = FetchingSchedule::Empty();
  StoreFetchingSchedule();
  ApplyPersistentFetchingSchedule();
}

void SchedulingRemoteSuggestionsProvider::ApplyPersistentFetchingSchedule() {
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

SchedulingRemoteSuggestionsProvider::FetchingSchedule
SchedulingRemoteSuggestionsProvider::GetDesiredFetchingSchedule() const {
  UserClassifier::UserClass user_class = user_classifier_->GetUserClass();

  FetchingSchedule schedule;
  schedule.interval_persistent_wifi =
      GetDesiredFetchingInterval(FetchingInterval::PERSISTENT_WIFI, user_class);
  schedule.interval_persistent_fallback = GetDesiredFetchingInterval(
      FetchingInterval::PERSISTENT_FALLBACK, user_class);
  schedule.interval_soft_on_usage_event = GetDesiredFetchingInterval(
      FetchingInterval::SOFT_ON_USAGE_EVENT, user_class);
  schedule.interval_soft_on_ntp_opened = GetDesiredFetchingInterval(
      FetchingInterval::SOFT_ON_NTP_OPENED, user_class);

  return schedule;
}

void SchedulingRemoteSuggestionsProvider::LoadLastFetchingSchedule() {
  schedule_.interval_persistent_wifi = base::TimeDelta::FromInternalValue(
      pref_service_->GetInt64(prefs::kSnippetPersistentFetchingIntervalWifi));
  schedule_.interval_persistent_fallback =
      base::TimeDelta::FromInternalValue(pref_service_->GetInt64(
          prefs::kSnippetPersistentFetchingIntervalFallback));
  schedule_.interval_soft_on_usage_event = base::TimeDelta::FromInternalValue(
      pref_service_->GetInt64(prefs::kSnippetSoftFetchingIntervalOnUsageEvent));
  schedule_.interval_soft_on_ntp_opened = base::TimeDelta::FromInternalValue(
      pref_service_->GetInt64(prefs::kSnippetSoftFetchingIntervalOnNtpOpened));
}

void SchedulingRemoteSuggestionsProvider::StoreFetchingSchedule() {
  pref_service_->SetInt64(prefs::kSnippetPersistentFetchingIntervalWifi,
                          schedule_.interval_persistent_wifi.ToInternalValue());
  pref_service_->SetInt64(
      prefs::kSnippetPersistentFetchingIntervalFallback,
      schedule_.interval_persistent_fallback.ToInternalValue());
  pref_service_->SetInt64(
      prefs::kSnippetSoftFetchingIntervalOnUsageEvent,
      schedule_.interval_soft_on_usage_event.ToInternalValue());
  pref_service_->SetInt64(
      prefs::kSnippetSoftFetchingIntervalOnNtpOpened,
      schedule_.interval_soft_on_ntp_opened.ToInternalValue());
}

void SchedulingRemoteSuggestionsProvider::RefetchInTheBackgroundIfEnabled(
    SchedulingRemoteSuggestionsProvider::TriggerType trigger) {
  if (BackgroundFetchesDisabled(trigger)) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.ContentSuggestions.BackgroundFetchTrigger",
      static_cast<int>(trigger), static_cast<int>(TriggerType::COUNT));

  RefetchInTheBackground(/*callback=*/nullptr);
}

bool SchedulingRemoteSuggestionsProvider::ShouldRefetchInTheBackgroundNow(
    SchedulingRemoteSuggestionsProvider::TriggerType trigger) {
  const base::Time last_fetch_attempt_time = base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSnippetLastFetchAttempt));
  base::Time first_allowed_fetch_time;
  switch (trigger) {
    case TriggerType::NTP_OPENED:
      first_allowed_fetch_time =
          last_fetch_attempt_time + schedule_.interval_soft_on_ntp_opened;
      break;
    case TriggerType::BROWSER_FOREGROUNDED:
    case TriggerType::BROWSER_COLD_START:
      first_allowed_fetch_time =
          last_fetch_attempt_time + schedule_.interval_soft_on_usage_event;
      break;
    case TriggerType::PERSISTENT_SCHEDULER_WAKE_UP:
    case TriggerType::COUNT:
      NOTREACHED();
      break;
  }
  base::Time now = clock_->Now();

  return background_fetches_allowed_after_ <= now &&
         first_allowed_fetch_time <= now;
}

bool SchedulingRemoteSuggestionsProvider::BackgroundFetchesDisabled(
    SchedulingRemoteSuggestionsProvider::TriggerType trigger) const {
  if (schedule_.is_empty()) {
    return true;  // Background fetches are disabled in general.
  }

  if (enabled_triggers_.count(trigger) == 0) {
    return true;  // Background fetches for |trigger| are not enabled.
  }
  return false;
}

bool SchedulingRemoteSuggestionsProvider::AcquireQuota(
    bool interactive_request) {
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

void SchedulingRemoteSuggestionsProvider::FetchFinished(
    const FetchDoneCallback& callback,
    Status fetch_status,
    std::vector<ContentSuggestion> suggestions) {
  OnFetchCompleted(fetch_status);
  if (callback) {
    callback.Run(fetch_status, std::move(suggestions));
  }
}

void SchedulingRemoteSuggestionsProvider::RefetchInTheBackgroundFinished(
    std::unique_ptr<FetchStatusCallback> callback,
    Status fetch_status) {
  background_fetch_in_progress_ = false;
  OnFetchCompleted(fetch_status);
  if (callback) {
    callback->Run(fetch_status);
  }
}

void SchedulingRemoteSuggestionsProvider::OnFetchCompleted(
    Status fetch_status) {
  pref_service_->SetInt64(prefs::kSnippetLastFetchAttempt,
                          clock_->Now().ToInternalValue());

  // Reschedule after a fetch. The persistent schedule is applied only after a
  // successful fetch. After a failed fetch, we want to keep the previous
  // persistent schedule intact so that we eventually get a persistent
  // fallback fetch (if the wifi persistent fetches keep failing).
  if (fetch_status.code != StatusCode::SUCCESS) {
    return;
  }
  ApplyPersistentFetchingSchedule();
}

void SchedulingRemoteSuggestionsProvider::ClearLastFetchAttemptTime() {
  pref_service_->ClearPref(prefs::kSnippetLastFetchAttempt);
}

std::set<SchedulingRemoteSuggestionsProvider::TriggerType>
SchedulingRemoteSuggestionsProvider::GetEnabledTriggerTypes() {
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

std::set<SchedulingRemoteSuggestionsProvider::TriggerType>
SchedulingRemoteSuggestionsProvider::GetDefaultEnabledTriggerTypes() {
  return {TriggerType::PERSISTENT_SCHEDULER_WAKE_UP, TriggerType::NTP_OPENED,
          TriggerType::BROWSER_FOREGROUNDED};
}

}  // namespace ntp_snippets
