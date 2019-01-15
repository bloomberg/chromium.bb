// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_H_

#include <stdint.h>

#include "base/callback.h"
#include "components/download/public/task/download_task_types.h"

namespace download {

using TaskFinishedCallback = base::OnceCallback<void(bool)>;

// A class to manage the calls made to the TaskScheduler, that abstracts away
// the details of the TaskScheduler from the calling code. The tasks can run
// independently of each other as long as they have different |task_type|.
// Scheduling another task of same |task_type| before the task is started will
// overwrite the params of the scheduled task.
class TaskManager {
 public:
  // Params used when scheduling a task through TaskScheduler::ScheduleTask().
  struct TaskParams {
    TaskParams();
    ~TaskParams() = default;
    bool operator==(const TaskParams& other) const;

    bool require_unmetered_network;
    bool require_charging;
    int optimal_battery_percentage;
    int64_t window_start_time_seconds;
    int64_t window_end_time_seconds;
  };

  TaskManager() = default;
  virtual ~TaskManager() = default;

  // Called to schedule a new task. Overwrites the params if a task of the same
  // type is already scheduled. If the task is currently running, it will cache
  // the params and schedule the task after the completion/stopping of the
  // current task.
  virtual void ScheduleTask(DownloadTaskType task_type,
                            const TaskParams& params) = 0;

  // Called to unschedule a scheduled task of the given type if it is not yet
  // started. Doesn't cancel the currently running task.
  virtual void UnscheduleTask(DownloadTaskType task_type) = 0;

  // Called when the system starts a scheduled task. The callback will be cached
  // by the class and run after receiving a call to NotifyTaskFinished().
  virtual void OnStartScheduledTask(DownloadTaskType task_type,
                                    TaskFinishedCallback callback) = 0;

  // Called when the system decides to stop an already running task.
  virtual void OnStopScheduledTask(DownloadTaskType task_type) = 0;

  // Should be called once the task is complete. The callback passed through
  // OnStartScheduledTask() will be run in order to notify that the task is done
  // and the system should reschedule the task with the original params if
  // |needs_reschedule| is true. If there are pending params for a new task, a
  // new task will be scheduled immediately and reschedule logic will not be
  // run.
  virtual void NotifyTaskFinished(DownloadTaskType task_type,
                                  bool needs_reschedule) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_MANAGER_H_
