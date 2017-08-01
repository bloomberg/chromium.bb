// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_snippets/remote/request_throttler.h"
#include "components/web_resource/eula_accepted_notifier.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace ntp_snippets {

struct Status;
class EulaState;
class UserClassifier;

// A client of RemoteSuggestionsProvider that introduces periodic fetching.
class RemoteSuggestionsSchedulerImpl : public RemoteSuggestionsScheduler {
 public:
  RemoteSuggestionsSchedulerImpl(PersistentScheduler* persistent_scheduler,
                                 const UserClassifier* user_classifier,
                                 PrefService* profile_prefs,
                                 PrefService* local_state_prefs,
                                 std::unique_ptr<base::Clock> clock);

  ~RemoteSuggestionsSchedulerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // RemoteSuggestionsScheduler implementation.
  void SetProvider(RemoteSuggestionsProvider* provider) override;
  void OnProviderActivated() override;
  void OnProviderDeactivated() override;
  void OnSuggestionsCleared() override;
  void OnHistoryCleared() override;
  void OnBrowserUpgraded() override;
  bool AcquireQuotaForInteractiveFetch() override;
  void OnInteractiveFetchFinished(Status fetch_status) override;
  void OnPersistentSchedulerWakeUp() override;
  void OnBrowserForegrounded() override;
  void OnBrowserColdStart() override;
  void OnSuggestionsSurfaceOpened() override;

 private:
  // Abstract description of the fetching schedule. See the enum
  // FetchingInterval for more documentation.
  struct FetchingSchedule {
    static FetchingSchedule Empty();
    bool operator==(const FetchingSchedule& other) const;
    bool operator!=(const FetchingSchedule& other) const;
    bool is_empty() const;

    base::TimeDelta interval_persistent_wifi;
    base::TimeDelta interval_persistent_fallback;
    base::TimeDelta interval_startup_wifi;
    base::TimeDelta interval_startup_fallback;
    base::TimeDelta interval_shown_wifi;
    base::TimeDelta interval_shown_fallback;
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

  // Trigger a background refetch for the given |trigger| if enabled and if the
  // timing is appropriate for another fetch.
  void RefetchInTheBackgroundIfAppropriate(TriggerType trigger);

  // Checks whether it is time to perform a soft background fetch for |trigger|,
  // according to |schedule|.
  bool ShouldRefetchInTheBackgroundNow(base::Time last_fetch_attempt_time,
                                       TriggerType trigger);

  // Returns whether background fetching (for the given |trigger|) is disabled.
  bool BackgroundFetchesDisabled(TriggerType trigger) const;

  // Returns true if quota is available for another request.
  bool AcquireQuota(bool interactive_request);

  // Callback after RefetchInTheBackground is completed.
  void RefetchInTheBackgroundFinished(Status fetch_status);

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

  // Interface for scheduling hard fetches, OS dependent. Not owned, may be
  // null.
  PersistentScheduler* persistent_scheduler_;

  // Interface for doing all the actual work (apart from scheduling). Not owned.
  RemoteSuggestionsProvider* provider_;

  FetchingSchedule schedule_;
  bool background_fetch_in_progress_;

  // Used to adapt the schedule based on usage activity of the user. Not owned.
  const UserClassifier* user_classifier_;

  // Request throttlers for limiting requests for different classes of users.
  RequestThrottler request_throttler_rare_ntp_user_;
  RequestThrottler request_throttler_active_ntp_user_;
  RequestThrottler request_throttler_active_suggestions_consumer_;

  // Variables to make sure we only report the first trigger of each kind to
  // UMA.
  bool time_until_first_shown_trigger_reported_;
  bool time_until_first_startup_trigger_reported_;

  // We should not fetch in background before EULA gets accepted.
  std::unique_ptr<EulaState> eula_state_;

  PrefService* profile_prefs_;
  std::unique_ptr<base::Clock> clock_;
  std::set<TriggerType> enabled_triggers_;

  base::Time background_fetches_allowed_after_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsSchedulerImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_IMPL_H_
