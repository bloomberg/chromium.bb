// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/scheduling_remote_suggestions_provider.h"

#include <string>
#include <utility>

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

// Default values for fetching intervals, fallback and wifi.
const double kDefaultFetchingIntervalRareNtpUser[] = {48.0, 24.0};
const double kDefaultFetchingIntervalActiveNtpUser[] = {24.0, 6.0};
const double kDefaultFetchingIntervalActiveSuggestionsConsumer[] = {24.0, 6.0};

// Variation parameters than can the default fetching intervals.
const char* kFetchingIntervalParamNameRareNtpUser[] = {
    "fetching_interval_hours-fallback-rare_ntp_user",
    "fetching_interval_hours-wifi-rare_ntp_user"};
const char* kFetchingIntervalParamNameActiveNtpUser[] = {
    "fetching_interval_hours-fallback-active_ntp_user",
    "fetching_interval_hours-wifi-active_ntp_user"};
const char* kFetchingIntervalParamNameActiveSuggestionsConsumer[] = {
    "fetching_interval_hours-fallback-active_suggestions_consumer",
    "fetching_interval_hours-wifi-active_suggestions_consumer"};

base::TimeDelta GetDesiredUpdateInterval(
    bool is_wifi,
    UserClassifier::UserClass user_class) {
  double default_value_hours = 0.0;

  const int index = is_wifi ? 1 : 0;
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

struct SchedulingRemoteSuggestionsProvider::FetchingSchedule {
  base::TimeDelta interval_wifi;
  base::TimeDelta interval_fallback;

  static FetchingSchedule Empty() {
    return FetchingSchedule{base::TimeDelta(),
                            base::TimeDelta()};
  }

  bool operator==(const FetchingSchedule& other) const {
    return interval_wifi == other.interval_wifi &&
           interval_fallback == other.interval_fallback;
  }

  bool operator!=(const FetchingSchedule& other) const {
    return !operator==(other);
  }

  bool is_empty() const {
    return interval_wifi.is_zero() && interval_fallback.is_zero();
  }
};

SchedulingRemoteSuggestionsProvider::SchedulingRemoteSuggestionsProvider(
    Observer* observer,
    std::unique_ptr<RemoteSuggestionsProvider> provider,
    PersistentScheduler* persistent_scheduler,
    const UserClassifier* user_classifier,
    PrefService* pref_service)
    : RemoteSuggestionsProvider(observer),
      RemoteSuggestionsScheduler(),
      provider_(std::move(provider)),
      persistent_scheduler_(persistent_scheduler),
      user_classifier_(user_classifier),
      pref_service_(pref_service) {
  DCHECK(user_classifier);
  DCHECK(pref_service);

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
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalWifi, 0);
  registry->RegisterInt64Pref(prefs::kSnippetBackgroundFetchingIntervalFallback,
                              0);
}

void SchedulingRemoteSuggestionsProvider::RescheduleFetching() {
  // Force the reschedule by stopping and starting it again.
  StopScheduling();
  StartScheduling();
}

void SchedulingRemoteSuggestionsProvider::OnPersistentSchedulerWakeUp() {
  provider_->RefetchInTheBackground(
      base::MakeUnique<RemoteSuggestionsProvider::FetchStatusCallback>(
          base::Bind(&SchedulingRemoteSuggestionsProvider::OnFetchCompleted,
                     base::Unretained(this))));
}

void SchedulingRemoteSuggestionsProvider::OnBrowserStartup() {
  // TODO(jkrcal): Implement.
}

void SchedulingRemoteSuggestionsProvider::OnNTPOpened() {
  // TODO(jkrcal): Implement.
}

void SchedulingRemoteSuggestionsProvider::SetProviderStatusCallback(
      std::unique_ptr<ProviderStatusCallback> callback) {
  provider_->SetProviderStatusCallback(std::move(callback));
}

void SchedulingRemoteSuggestionsProvider::RefetchInTheBackground(
    std::unique_ptr<FetchStatusCallback> callback) {
  provider_->RefetchInTheBackground(std::move(callback));
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
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!persistent_scheduler_) {
    return;
  }

  FetchingSchedule last_schedule = GetLastFetchingSchedule();
  FetchingSchedule schedule = GetDesiredFetchingSchedule();

  // Reset the schedule only if the parameters have changed.
  if (last_schedule != schedule) {
    ApplyFetchingSchedule(schedule);
  }
}

void SchedulingRemoteSuggestionsProvider::StopScheduling() {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!persistent_scheduler_) {
    return;
  }

  // Do not unschedule if already switched off
  FetchingSchedule last_schedule = GetLastFetchingSchedule();
  if (last_schedule.is_empty()) {
    return;
  }

  persistent_scheduler_->Unschedule();

  StoreLastFetchingSchedule(FetchingSchedule::Empty());
}

void SchedulingRemoteSuggestionsProvider::ApplyFetchingSchedule(
    const FetchingSchedule& schedule) {
  persistent_scheduler_->Schedule(schedule.interval_wifi,
                                  schedule.interval_fallback);

  StoreLastFetchingSchedule(schedule);
}

SchedulingRemoteSuggestionsProvider::FetchingSchedule
SchedulingRemoteSuggestionsProvider::GetDesiredFetchingSchedule() const {
  UserClassifier::UserClass user_class = user_classifier_->GetUserClass();

  FetchingSchedule schedule;
  schedule.interval_wifi =
      GetDesiredUpdateInterval(/*is_wifi=*/true, user_class);
  schedule.interval_fallback =
      GetDesiredUpdateInterval(/*is_wifi=*/false, user_class);
  return schedule;
}

SchedulingRemoteSuggestionsProvider::FetchingSchedule
SchedulingRemoteSuggestionsProvider::GetLastFetchingSchedule() const {
  FetchingSchedule schedule;
  schedule.interval_wifi = base::TimeDelta::FromInternalValue(
      pref_service_->GetInt64(prefs::kSnippetBackgroundFetchingIntervalWifi));
  schedule.interval_fallback =
      base::TimeDelta::FromInternalValue(pref_service_->GetInt64(
          prefs::kSnippetBackgroundFetchingIntervalFallback));
  return schedule;
}

void SchedulingRemoteSuggestionsProvider::StoreLastFetchingSchedule(
    const FetchingSchedule& schedule) {
  pref_service_->SetInt64(
      prefs::kSnippetBackgroundFetchingIntervalWifi,
      schedule.interval_wifi.ToInternalValue());
  pref_service_->SetInt64(
      prefs::kSnippetBackgroundFetchingIntervalFallback,
      schedule.interval_fallback.ToInternalValue());
}

void SchedulingRemoteSuggestionsProvider::FetchFinished(
    const FetchDoneCallback& callback,
    Status status_code,
    std::vector<ContentSuggestion> suggestions) {
  OnFetchCompleted(status_code);
  callback.Run(status_code, std::move(suggestions));
}

void SchedulingRemoteSuggestionsProvider::OnFetchCompleted(
    Status fetch_status) {
  // The scheduler only exists on Android so far, it's null on other platforms.
  if (!persistent_scheduler_) {
    return;
  }

  if (fetch_status.code != StatusCode::SUCCESS) {
    return;
  }

  // Reschedule after a successful fetch. This resets all currently scheduled
  // fetches, to make sure the fallback interval triggers only if no wifi fetch
  // succeeded, and also that we don't do a background fetch immediately after
  // a user-initiated one.
  ApplyFetchingSchedule(GetLastFetchingSchedule());
}

}  // namespace ntp_snippets
