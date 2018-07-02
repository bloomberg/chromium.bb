// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
#define COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class Time;
class TimeDelta;
}  // namespace base

namespace feed {

// The enum values and names are kept in sync with SchedulerApi.RequestBehavior
// through Java unit tests, new values however must be manually added. If any
// new values are added, also update FeedSchedulerBridgeTest.java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.feed
enum NativeRequestBehavior {
  UNKNOWN = 0,
  REQUEST_WITH_WAIT,
  REQUEST_WITH_CONTENT,
  REQUEST_WITH_TIMEOUT,
  NO_REQUEST_WITH_WAIT,
  NO_REQUEST_WITH_CONTENT,
  NO_REQUEST_WITH_TIMEOUT
};

// Implementation of the Feed Scheduler Host API. The scheduler host decides
// what content is allowed to be shown, based on its age, and when to fetch new
// content.
class FeedSchedulerHost {
 public:
  FeedSchedulerHost(PrefService* pref_service, base::Clock* clock);
  ~FeedSchedulerHost();

  using ScheduleBackgroundTaskCallback =
      base::RepeatingCallback<void(base::TimeDelta)>;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Provide dependent pieces of functionality the scheduler relies on. Should
  // be called exactly once before other public methods are called. This is
  // separate from the constructor because the FeedHostService owns and creates
  // this class, while these providers need to be bridged through the JNI into a
  // specific place that the FeedHostService does not know of.
  void Initialize(
      base::RepeatingClosure refresh_callback,
      ScheduleBackgroundTaskCallback schedule_background_task_callback);

  // Called when the NTP is opened to decide how to handle displaying and
  // refreshing content.
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      base::Time content_creation_date_time,
      bool has_outstanding_request);

  // Called when a successful refresh completes.
  void OnReceiveNewContent(base::Time content_creation_date_time);

  // Called when an unsuccessful refresh completes.
  void OnRequestError(int network_response_code);

  // Called when browser is opened, launched, or foregrounded.
  void OnForegrounded();

  // Called when the scheduled fixed timer wakes up, |on_completion| will be
  // invoked when the refresh completes, regardless of success. If no refresh
  // is started for this trigger, |on_completion| is run immediately.
  void OnFixedTimer(base::OnceClosure on_completion);

  // Called when the background task may need to be rescheduled, such as on OS
  // upgrades that change the way tasks are stored.
  void OnTaskReschedule();

 private:
  // The TriggerType enum specifies values for the events that can trigger
  // refreshing articles.
  enum class TriggerType;

  // Determines whether a refresh should be performed for the given |trigger|.
  // If this method is called and returns true we presume the refresh will
  // happen, therefore we report metrics respectively.
  bool ShouldRefresh(TriggerType trigger);

  // Decides if content whose age is the difference between now and
  // |content_creation_date_time| is old enough to be considered stale.
  bool IsContentStale(base::Time content_creation_date_time);

  // Returns the time threshold for content or previous refresh attempt to be
  // considered old enough for a given trigger to warrant a refresh.
  base::TimeDelta GetTriggerThreshold(TriggerType trigger);

  // Schedules a task to wakeup and try to refresh. Overrides previously
  // scheduled tasks.
  void ScheduleFixedTimerWakeUp(base::TimeDelta period);

  // Non-owning reference to pref service providing durable storage.
  PrefService* pref_service_;

  // Non-owning reference to clock to get current time.
  base::Clock* clock_;

  // Callback to request that an async refresh be started.
  base::RepeatingClosure refresh_callback_;

  // Provides ability to schedule and cancel persistent background fixed timer
  // wake ups that will call into OnFixedTimer().
  ScheduleBackgroundTaskCallback schedule_background_task_callback_;

  // When a background wake up has caused a fixed timer refresh, this callback
  // will be valid and holds a way to inform the task driving this wake up that
  // the refresh has completed. Is called on refresh success or failure.
  base::OnceClosure fixed_timer_completion_;

  DISALLOW_COPY_AND_ASSIGN(FeedSchedulerHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_SCHEDULER_HOST_H_
