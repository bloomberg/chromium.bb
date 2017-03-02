// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_SCHEDULING_REMOTE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_SCHEDULING_REMOTE_SUGGESTIONS_PROVIDER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_snippets/remote/request_throttler.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace ntp_snippets {

struct Status;
class UserClassifier;

// A wrapper around RemoteSuggestionsProvider that introduces periodic fetching.
//
// The class initiates fetches on its own in these situations:
//  - initial fetch when the provider is constructed and we have no suggestions;
//  - regular fetches according to its schedule.
// TODO(jkrcal): After soft fetch on Chrome startup is introduced, remove
// the initial fetch completely.
//
// The class also needs to understand when last fetch trials and successful
// fetches happen and thus it intercepts following interactive fetch requests:
//  - Fetch() - after "More" button of a remote section is pressed in the UI;
// TODO(jkrcal): Clarify what Fetch() should do for this provider and maybe stop
// intercepting it.
// TODO(jkrcal): Intercept also ReloadSuggestions() call (after the user swipes
// away everything incl. all empty sections and presses "More"); Not done in the
// first shot because  it implements a public interface function without any
// callback.
// This class is final because it does things in its constructor which make it
// unsafe to derive from it.
// TODO(jkrcal): Introduce two-phase initialization and make the class not
// final? (see the same comment for RemoteSuggestionsProvider)
// TODO(jkrcal): Change the interface to ContentSuggestionsProvider. We do not
// need any special functionality, all special should be exposed in the
// Scheduler interface. crbug.com/695447
class SchedulingRemoteSuggestionsProvider final
    : public RemoteSuggestionsProvider,
      public RemoteSuggestionsScheduler {
 public:
  SchedulingRemoteSuggestionsProvider(
      Observer* observer,
      std::unique_ptr<RemoteSuggestionsProvider> provider,
      PersistentScheduler* persistent_scheduler,
      const UserClassifier* user_classifier,
      PrefService* pref_service,
      std::unique_ptr<base::Clock> clock);

  ~SchedulingRemoteSuggestionsProvider() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // RemoteSuggestionsScheduler implementation.
  void OnProviderActivated() override;
  void OnProviderDeactivated() override;
  void OnSuggestionsCleared() override;
  void OnHistoryCleared() override;
  void RescheduleFetching() override;
  void OnPersistentSchedulerWakeUp() override;
  void OnBrowserForegrounded() override;
  void OnBrowserColdStart() override;
  void OnNTPOpened() override;

  // RemoteSuggestionsProvider implementation.
  void RefetchInTheBackground(
      std::unique_ptr<FetchStatusCallback> callback) override;
  const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            const ImageFetchedCallback& callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             const FetchDoneCallback& callback) override;
  void ReloadSuggestions() override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions(Category category) override;
  void OnSignInStateChanged() override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      const DismissedSuggestionsCallback& callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

 private:
  // Abstract description of the fetching schedule.
  struct FetchingSchedule {
      static FetchingSchedule Empty();
      bool operator==(const FetchingSchedule& other) const;
      bool operator!=(const FetchingSchedule& other) const;
      bool is_empty() const;

      base::TimeDelta interval_persistent_wifi;
      base::TimeDelta interval_persistent_fallback;
      base::TimeDelta interval_soft_on_usage_event;
      base::TimeDelta interval_soft_on_ntp_opened;
  };

  enum class TriggerType;

  // After the call, updates will be scheduled in the future. Idempotent, can be
  // run any time later without impacting the current schedule.
  // If you want to enforce rescheduling, call Unschedule() and then Schedule().
  void StartScheduling();

  // After the call, no updates will happen before another call to Schedule().
  // Idempotent, can be run any time later without impacting the current
  // schedule.
  void StopScheduling();

  // Trigger a background refetch for the given |trigger| if enabled.
  void RefetchInTheBackgroundIfEnabled(TriggerType trigger);

  // Checks whether it is time to perform a soft background fetch, according to
  // |schedule|.
  bool ShouldRefetchInTheBackgroundNow(TriggerType trigger);

  // Returns whether background fetching (for the given |trigger|) is disabled.
  bool BackgroundFetchesDisabled(TriggerType trigger) const;

  // Returns true if quota is available for another request.
  bool AcquireQuota(bool interactive_request);

  // Callback after Fetch is completed.
  void FetchFinished(const FetchDoneCallback& callback,
                     Status fetch_status,
                     std::vector<ContentSuggestion> suggestions);

  // Callback after RefetchInTheBackground is completed.
  void RefetchInTheBackgroundFinished(
      std::unique_ptr<FetchStatusCallback> callback,
      Status fetch_status);

  // Common function to call after a fetch of any type is finished.
  void OnFetchCompleted(Status fetch_status);

  // Clears the time of the last fetch so that the provider is ready to make a
  // soft fetch at any later time (upon a trigger).
  void ClearLastFetchAttemptTime();

  FetchingSchedule GetDesiredFetchingSchedule() const;

  // Load and store |schedule_|.
  void LoadLastFetchingSchedule();
  void StoreFetchingSchedule();

  // Applies the persistent schedule given by |schedule_|.
  void ApplyPersistentFetchingSchedule();

  // Gets enabled trigger types from the variation parameter.
  std::set<TriggerType> GetEnabledTriggerTypes();

  // Gets trigger types enabled by default.
  std::set<TriggerType> GetDefaultEnabledTriggerTypes();

  // Interface for doing all the actual work (apart from scheduling).
  std::unique_ptr<RemoteSuggestionsProvider> provider_;

  // Interface for scheduling hard fetches, OS dependent. Not owned, may be
  // null.
  PersistentScheduler* persistent_scheduler_;

  FetchingSchedule schedule_;
  bool background_fetch_in_progress_;

  // Used to adapt the schedule based on usage activity of the user. Not owned.
  const UserClassifier* user_classifier_;

  // Request throttlers for limiting requests for different classes of users.
  RequestThrottler request_throttler_rare_ntp_user_;
  RequestThrottler request_throttler_active_ntp_user_;
  RequestThrottler request_throttler_active_suggestions_consumer_;

  PrefService* pref_service_;
  std::unique_ptr<base::Clock> clock_;
  std::set<SchedulingRemoteSuggestionsProvider::TriggerType> enabled_triggers_;

  base::Time background_fetches_allowed_after_;

  DISALLOW_COPY_AND_ASSIGN(SchedulingRemoteSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_SCHEDULING_REMOTE_SUGGESTIONS_PROVIDER_H_
