// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/scheduling_remote_suggestions_provider.h"

#include <random>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

enum class FetchingInterval {
  PERSISTENT_FALLBACK,
  PERSISTENT_WIFI,
  SOFT_ON_USAGE_EVENT,
  COUNT
};

// Default values for fetching intervals, fallback and wifi.
const double kDefaultFetchingIntervalRareNtpUser[] = {48.0, 24.0, 12.0};
const double kDefaultFetchingIntervalActiveNtpUser[] = {24.0, 6.0, 2.0};
const double kDefaultFetchingIntervalActiveSuggestionsConsumer[] = {24.0, 6.0,
                                                                    2.0};

// Variation parameters than can the default fetching intervals.
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user",
    "soft_fetching_interval_hours-active-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user",
    "soft_fetching_interval_hours-active-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer",
    "soft_fetching_interval_hours-active-active_suggestions_consumer"};

static_assert(
    static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kDefaultFetchingIntervalActiveSuggestionsConsumer) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameRareNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameActiveNtpUser) &&
        static_cast<unsigned int>(FetchingInterval::COUNT) ==
            arraysize(kFetchingIntervalParamNameActiveSuggestionsConsumer),
    "Fill in all the info for fetching intervals.");

base::TimeDelta GetDesiredFetchingInterval(
    FetchingInterval interval,
    UserClassifier::UserClass user_class) {
  double default_value_hours = 0.0;

  DCHECK(interval != FetchingInterval::COUNT);
  const unsigned int index = static_cast<unsigned int>(interval);
  DCHECK(index < arraysize(kDefaultFetchingIntervalRareNtpUser));

  const char* param_name = nullptr;
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      default_value_hours = kDefaultFetchingIntervalRareNtpUser[index];
      param_name = kFetchingIntervalParamNameRareNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      default_value_hours = kDefaultFetchingIntervalActiveNtpUser[index];
      param_name = kFetchingIntervalParamNameActiveNtpUser[index];
      break;
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      default_value_hours =
          kDefaultFetchingIntervalActiveSuggestionsConsumer[index];
      param_name = kFetchingIntervalParamNameActiveSuggestionsConsumer[index];
      break;
  }

  double value_hours = variations::GetVariationParamByFeatureAsDouble(
      ntp_snippets::kArticleSuggestionsFeature, param_name,
      default_value_hours);

  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

}  // namespace

// static
SchedulingRemoteSuggestionsProvider::FetchingSchedule
SchedulingRemoteSuggestionsProvider::FetchingSchedule::Empty() {
  return FetchingSchedule{base::TimeDelta(), base::TimeDelta(),
                          base::TimeDelta()};
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::operator==(
    const FetchingSchedule& other) const {
  return interval_persistent_wifi == other.interval_persistent_wifi &&
         interval_persistent_fallback == other.interval_persistent_fallback &&
         interval_soft_on_usage_event == other.interval_soft_on_usage_event;
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::operator!=(
    const FetchingSchedule& other) const {
  return !operator==(other);
}

bool SchedulingRemoteSuggestionsProvider::FetchingSchedule::is_empty() const {
  return interval_persistent_wifi.is_zero() &&
         interval_persistent_fallback.is_zero() &&
         interval_soft_on_usage_event.is_zero();
}

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
      pref_service_(pref_service),
      clock_(std::move(clock)) {
  DCHECK(user_classifier);
  DCHECK(pref_service);

  LoadLastFetchingSchedule();

  provider_->SetProviderStatusCallback(
      base::MakeUnique<RemoteSuggestionsProvider::ProviderStatusCallback>(
          base::BindRepeating(
              &SchedulingRemoteSuggestionsProvider::OnProviderStatusChanged,
              base::Unretained(this))));
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
}

void SchedulingRemoteSuggestionsProvider::RescheduleFetching() {
  // Force the reschedule by stopping and starting it again.
  StopScheduling();
  StartScheduling();
}

void SchedulingRemoteSuggestionsProvider::OnPersistentSchedulerWakeUp() {
  if (BackgroundFetchesDisabled()) {
    return;
  }

  RefetchInTheBackground(/*callback=*/nullptr);
}

void SchedulingRemoteSuggestionsProvider::OnBrowserForegrounded() {
  // TODO(jkrcal): Consider that this is called whenever we open or return to an
  // Activity. Therefore, keep work light for fast start up calls.
  // TODO(jkrcal): Implement.
}

void SchedulingRemoteSuggestionsProvider::OnBrowserColdStart() {
  // TODO(fhorschig|jkrcal): Consider that work here must be kept light for fast
  // cold start ups.
  // TODO(jkrcal): Implement.
}

void SchedulingRemoteSuggestionsProvider::OnNTPOpened() {
  if (BackgroundFetchesDisabled() || !ShouldRefetchInTheBackgroundNow()) {
    return;
  }

  RefetchInTheBackground(/*callback=*/nullptr);
}

void SchedulingRemoteSuggestionsProvider::SetProviderStatusCallback(
    std::unique_ptr<ProviderStatusCallback> callback) {
  provider_->SetProviderStatusCallback(std::move(callback));
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

  background_fetch_in_progress_ = true;
  RemoteSuggestionsProvider::FetchStatusCallback wrapper_callback = base::Bind(
      &SchedulingRemoteSuggestionsProvider::RefetchInTheBackgroundFinished,
      base::Unretained(this), base::Passed(&callback));
  provider_->RefetchInTheBackground(
      base::MakeUnique<RemoteSuggestionsProvider::FetchStatusCallback>(
          std::move(wrapper_callback)));
}

const NTPSnippetsFetcher* SchedulingRemoteSuggestionsProvider::
    snippets_fetcher_for_testing_and_debugging() const {
  return provider_->snippets_fetcher_for_testing_and_debugging();
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
  provider_->Fetch(
      category, known_suggestion_ids,
      base::Bind(&SchedulingRemoteSuggestionsProvider::FetchFinished,
                 base::Unretained(this), callback));
}

void SchedulingRemoteSuggestionsProvider::ReloadSuggestions() {
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

void SchedulingRemoteSuggestionsProvider::OnProviderStatusChanged(
    RemoteSuggestionsProvider::ProviderStatus status) {
  switch (status) {
    case RemoteSuggestionsProvider::ProviderStatus::ACTIVE:
      StartScheduling();
      return;
    case RemoteSuggestionsProvider::ProviderStatus::INACTIVE:
      StopScheduling();
      return;
  }
  NOTREACHED();
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
}

bool SchedulingRemoteSuggestionsProvider::BackgroundFetchesDisabled() const {
  return schedule_.is_empty();
}

bool SchedulingRemoteSuggestionsProvider::ShouldRefetchInTheBackgroundNow() {
  base::Time first_allowed_fetch_time =
      base::Time::FromInternalValue(
          pref_service_->GetInt64(prefs::kSnippetLastFetchAttempt)) +
      schedule_.interval_soft_on_usage_event;
  return first_allowed_fetch_time <= clock_->Now();
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

}  // namespace ntp_snippets
