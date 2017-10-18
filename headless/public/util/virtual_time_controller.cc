// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/virtual_time_controller.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"

namespace headless {

using base::TimeDelta;

VirtualTimeController::VirtualTimeController(
    HeadlessDevToolsClient* devtools_client,
    int max_task_starvation_count)
    : devtools_client_(devtools_client),
      max_task_starvation_count_(max_task_starvation_count) {
  devtools_client_->GetEmulation()->GetExperimental()->AddObserver(this);
}

VirtualTimeController::~VirtualTimeController() {
  devtools_client_->GetEmulation()->GetExperimental()->RemoveObserver(this);
}

void VirtualTimeController::GrantVirtualTimeBudget(
    emulation::VirtualTimePolicy policy,
    int budget_ms,
    const base::Callback<void()>& set_up_complete_callback,
    const base::Callback<void()>& budget_expired_callback) {
  DCHECK(!set_up_complete_callback_);
  DCHECK(!budget_expired_callback_);

  set_up_complete_callback_ = set_up_complete_callback;
  budget_expired_callback_ = budget_expired_callback;
  requested_budget_ = TimeDelta::FromMilliseconds(budget_ms);
  accumulated_time_ = TimeDelta();
  virtual_time_policy_ = policy;

  // Notify tasks of new budget request.
  for (TaskEntry& entry : tasks_)
    NotifyTaskBudgetRequested(&entry, budget_ms);

  // If there tasks, NotifyTasksAndAdvance is called from TaskReadyToAdvance.
  if (tasks_.empty())
    NotifyTasksAndAdvance();
}

void VirtualTimeController::NotifyTasksAndAdvance() {
  // The task may call its continue callback synchronously. Prevent re-entrance.
  if (in_notify_tasks_and_advance_)
    return;

  base::AutoReset<bool> reset(&in_notify_tasks_and_advance_, true);

  DCHECK(budget_expired_callback_);

  // Give at most as much virtual time as available until the next callback.
  bool ready_to_advance = true;
  TimeDelta next_budget = requested_budget_ - accumulated_time_;
  for (TaskEntry& entry : tasks_) {
    if (entry.next_execution_time <= total_elapsed_time_)
      NotifyTaskIntervalElapsed(&entry);

    ready_to_advance &= entry.ready_to_advance;
    next_budget =
        std::min(next_budget, entry.next_execution_time - total_elapsed_time_);
  }

  if (!ready_to_advance)
    return;

  if (accumulated_time_ >= requested_budget_) {
    for (const TaskEntry& entry : tasks_)
      entry.task->BudgetExpired(total_elapsed_time_);

    auto callback = budget_expired_callback_;
    budget_expired_callback_.Reset();
    callback.Run();
    return;
  }

  SetVirtualTimePolicy(next_budget);
}

void VirtualTimeController::NotifyTaskIntervalElapsed(TaskEntry* entry) {
  entry->ready_to_advance = false;
  entry->next_execution_time = total_elapsed_time_ + entry->interval;

  entry->task->IntervalElapsed(
      total_elapsed_time_,
      base::Bind(&VirtualTimeController::TaskReadyToAdvance,
                 base::Unretained(this), base::Unretained(entry)));
}

void VirtualTimeController::NotifyTaskBudgetRequested(TaskEntry* entry,
                                                      int budget_ms) {
  entry->ready_to_advance = false;
  entry->task->BudgetRequested(
      total_elapsed_time_, budget_ms,
      base::Bind(&VirtualTimeController::TaskReadyToAdvance,
                 base::Unretained(this), base::Unretained(entry)));
}

void VirtualTimeController::TaskReadyToAdvance(TaskEntry* entry) {
  entry->ready_to_advance = true;
  NotifyTasksAndAdvance();
}

void VirtualTimeController::SetVirtualTimePolicy(const TimeDelta& next_budget) {
  last_used_budget_ = next_budget;
  virtual_time_active_ = true;
  devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
      emulation::SetVirtualTimePolicyParams::Builder()
          .SetPolicy(virtual_time_policy_)
          .SetBudget(next_budget.InMilliseconds())
          .SetMaxVirtualTimeTaskStarvationCount(max_task_starvation_count_)
          .Build(),
      base::Bind(&VirtualTimeController::SetVirtualTimePolicyDone,
                 base::Unretained(this)));
}

void VirtualTimeController::SetVirtualTimePolicyDone(
    std::unique_ptr<emulation::SetVirtualTimePolicyResult>) {
  if (set_up_complete_callback_) {
    set_up_complete_callback_.Run();
    set_up_complete_callback_.Reset();
  }
}

void VirtualTimeController::OnVirtualTimeBudgetExpired(
    const emulation::VirtualTimeBudgetExpiredParams& params) {
  // As the DevTools script may interfere with virtual time directly, we cannot
  // assume that this event is due to AdvanceVirtualTime().
  if (!budget_expired_callback_)
    return;

  accumulated_time_ += last_used_budget_;
  total_elapsed_time_ += last_used_budget_;
  virtual_time_active_ = false;
  NotifyTasksAndAdvance();
}

void VirtualTimeController::ScheduleRepeatingTask(RepeatingTask* task,
                                                  int interval_ms) {
  if (virtual_time_active_) {
    // We cannot accurately modify any previously granted virtual time budget.
    LOG(WARNING) << "VirtualTimeController tasks should be added while "
                    "virtual time is paused.";
  }

  TaskEntry entry;
  entry.task = task;
  entry.interval = TimeDelta::FromMilliseconds(interval_ms);
  entry.next_execution_time = total_elapsed_time_ + entry.interval;
  tasks_.push_back(entry);
}

void VirtualTimeController::CancelRepeatingTask(RepeatingTask* task) {
  tasks_.remove_if(
      [task](const TaskEntry& entry) { return entry.task == task; });
}

}  // namespace headless
