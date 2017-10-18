// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_VIRTUAL_TIME_CONTROLLER_H_
#define HEADLESS_PUBLIC_UTIL_VIRTUAL_TIME_CONTROLLER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_export.h"

namespace headless {

// Controls how virtual time progresses. RepeatingTasks can register their
// interest to be periodically notified about changes to the current virtual
// time.
class HEADLESS_EXPORT VirtualTimeController
    : public emulation::ExperimentalObserver {
 public:
  VirtualTimeController(HeadlessDevToolsClient* devtools_client,
                        int max_task_starvation_count = 0);
  ~VirtualTimeController() override;

  // Grants |budget_ms| milliseconds of virtual time by applying the provided
  // |policy|.
  //
  // |set_up_complete_callback|, if set, is run after the (initial) policy was
  // applied via DevTools. |budget_expired_callback| will be called when the
  // budget has been exhausted.
  //
  // Should not be called again until the previous invocation's
  // budget_expired_callback was executed.
  void GrantVirtualTimeBudget(
      emulation::VirtualTimePolicy policy,
      int budget_ms,
      const base::Callback<void()>& set_up_complete_callback,
      const base::Callback<void()>& budget_expired_callback);

  class RepeatingTask {
   public:
    virtual ~RepeatingTask() {}

    // Called when the tasks's requested virtual time interval has elapsed.
    // |virtual_time| is the virtual time duration that has advanced since the
    // page started loading (millisecond granularity). The task should call
    // |continue_callback| when it is ready for virtual time to continue
    // advancing.
    virtual void IntervalElapsed(
        const base::TimeDelta& virtual_time,
        const base::Callback<void()>& continue_callback) = 0;

    // Called when a new virtual time budget grant was requested. The task
    // should call |continue_callback| when it is ready for virtual time to
    // continue advancing.
    virtual void BudgetRequested(
        const base::TimeDelta& virtual_time,
        int requested_budget_ms,
        const base::Callback<void()>& continue_callback) = 0;

    // Called when the latest virtual time budget has been used up.
    virtual void BudgetExpired(const base::TimeDelta& virtual_time) = 0;
  };

  // Interleaves execution of the provided |task| with advancing of virtual
  // time. The task will be notified whenever another |interval_ms| milliseconds
  // of virtual time have elapsed, as well as when the last granted budget has
  // been used up.
  //
  // To ensure that the task is notified of elapsed intervals accurately, it
  // should be added while virtual time is paused.
  void ScheduleRepeatingTask(RepeatingTask* task, int interval_ms);
  void CancelRepeatingTask(RepeatingTask* task);

 private:
  struct TaskEntry {
    RepeatingTask* task;
    base::TimeDelta interval;
    base::TimeDelta next_execution_time;
    bool ready_to_advance = true;
  };

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override;

  void NotifyTasksAndAdvance();
  void NotifyTaskIntervalElapsed(TaskEntry* entry);
  void NotifyTaskBudgetRequested(TaskEntry* entry, int budget_ms);
  void TaskReadyToAdvance(TaskEntry* entry);

  void SetVirtualTimePolicy(const base::TimeDelta& next_budget);
  void SetVirtualTimePolicyDone(
      std::unique_ptr<emulation::SetVirtualTimePolicyResult>);

  HeadlessDevToolsClient* const devtools_client_;  // NOT OWNED
  const int max_task_starvation_count_;

  emulation::VirtualTimePolicy virtual_time_policy_ =
      emulation::VirtualTimePolicy::ADVANCE;
  base::Callback<void()> set_up_complete_callback_;
  base::Callback<void()> budget_expired_callback_;

  bool virtual_time_active_ = false;
  base::TimeDelta total_elapsed_time_;
  base::TimeDelta requested_budget_;
  base::TimeDelta last_used_budget_;
  base::TimeDelta accumulated_time_;

  std::list<TaskEntry> tasks_;
  bool in_notify_tasks_and_advance_ = false;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_VIRTUAL_TIME_CONTROLLER_H_
