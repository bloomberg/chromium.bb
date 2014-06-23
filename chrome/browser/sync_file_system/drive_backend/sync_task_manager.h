// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/task_logger.h"

namespace base {
class SequencedTaskRunner;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {
namespace drive_backend {

class SyncTask;
class SyncTaskToken;
struct BlockingFactor;

// This class manages asynchronous tasks for Sync FileSystem.  Each task must be
// either a Task or a SyncTask.
// The instance runs single task as the foreground task, and multiple tasks as
// background tasks.  Running background task has a BlockingFactor that
// describes which task can run in parallel.  When a task start running as a
// background task, SyncTaskManager checks if any running background task
// doesn't block the new background task, and queues it up if it can't run.
class SyncTaskManager : public base::SupportsWeakPtr<SyncTaskManager> {
 public:
  typedef base::Callback<void(const SyncStatusCallback& callback)> Task;
  typedef base::Callback<void(scoped_ptr<SyncTaskToken> token)> Continuation;

  enum Priority {
    PRIORITY_LOW,
    PRIORITY_MED,
    PRIORITY_HIGH,
  };

  class Client {
   public:
    virtual ~Client() {}

    // Called when the manager is idle.
    virtual void MaybeScheduleNextTask() = 0;

    // Called when the manager is notified a task is done.
    virtual void NotifyLastOperationStatus(
        SyncStatusCode last_operation_status,
        bool last_operation_used_network) = 0;

    virtual void RecordTaskLog(scoped_ptr<TaskLogger::TaskLog> task_log) = 0;
  };

  // Runs at most |maximum_background_tasks| parallel as background tasks.
  // If |maximum_background_tasks| is zero, all task runs as foreground task.
  SyncTaskManager(base::WeakPtr<Client> client,
                  size_t maximum_background_task,
                  base::SequencedTaskRunner* task_runner);
  virtual ~SyncTaskManager();

  // This needs to be called to start task scheduling.
  // If |status| is not SYNC_STATUS_OK calling this may change the
  // service status. This should not be called more than once.
  void Initialize(SyncStatusCode status);

  // Schedules a task at the given priority.
  void ScheduleTask(const tracked_objects::Location& from_here,
                    const Task& task,
                    Priority priority,
                    const SyncStatusCallback& callback);
  void ScheduleSyncTask(const tracked_objects::Location& from_here,
                        scoped_ptr<SyncTask> task,
                        Priority priority,
                        const SyncStatusCallback& callback);

  // Runs the posted task only when we're idle.  Returns true if tha task is
  // scheduled.
  bool ScheduleTaskIfIdle(const tracked_objects::Location& from_here,
                          const Task& task,
                          const SyncStatusCallback& callback);
  bool ScheduleSyncTaskIfIdle(const tracked_objects::Location& from_here,
                              scoped_ptr<SyncTask> task,
                              const SyncStatusCallback& callback);

  // Notifies SyncTaskManager that the task associated to |token| has finished
  // with |status|.
  static void NotifyTaskDone(scoped_ptr<SyncTaskToken> token,
                             SyncStatusCode status);

  // Updates |blocking_factor| associated to the current task by specified
  // |blocking_factor| and turns the current task to a background task if
  // the current task is running as a foreground task.
  // If specified |blocking_factor| is blocked by any other blocking factor
  // associated to an existing background task, this function waits for the
  // existing background task to finish.
  // Upon the task is ready to run as a background task, calls |continuation|
  // with new SyncTaskToken.
  // Note that this function once releases previous |blocking_factor| before
  // applying new |blocking_factor|.  So, any other task may be run before
  // invocation of |continuation|.
  static void UpdateBlockingFactor(scoped_ptr<SyncTaskToken> current_task_token,
                                   scoped_ptr<BlockingFactor> blocking_factor,
                                   const Continuation& continuation);

  bool IsRunningTask(int64 task_token_id) const;

  void DetachFromSequence();

 private:
  struct PendingTask {
    base::Closure task;
    Priority priority;
    int64 seq;

    PendingTask();
    PendingTask(const base::Closure& task, Priority pri, int seq);
    ~PendingTask();
  };

  struct PendingTaskComparator {
    bool operator()(const PendingTask& left,
                    const PendingTask& right) const;
  };

  // Non-static version of NotifyTaskDone.
  void NotifyTaskDoneBody(scoped_ptr<SyncTaskToken> token,
                          SyncStatusCode status);

  // Non-static version of UpdateBlockingFactor.
  void UpdateBlockingFactorBody(scoped_ptr<SyncTaskToken> foreground_task_token,
                                scoped_ptr<SyncTaskToken> background_task_token,
                                scoped_ptr<TaskLogger::TaskLog> task_log,
                                scoped_ptr<BlockingFactor> blocking_factor,
                                const Continuation& continuation);

  // This should be called when an async task needs to get a task token.
  scoped_ptr<SyncTaskToken> GetToken(const tracked_objects::Location& from_here,
                                     const SyncStatusCallback& callback);

  scoped_ptr<SyncTaskToken> GetTokenForBackgroundTask(
      const tracked_objects::Location& from_here,
      const SyncStatusCallback& callback,
      scoped_ptr<BlockingFactor> blocking_factor);

  void PushPendingTask(const base::Closure& closure, Priority priority);

  void RunTask(scoped_ptr<SyncTaskToken> token,
               scoped_ptr<SyncTask> task);

  // Runs a pending task as a foreground task if possible.
  // If |token| is non-NULL, put |token| back to |token_| beforehand.
  void MaybeStartNextForegroundTask(scoped_ptr<SyncTaskToken> token);

  base::WeakPtr<Client> client_;

  // Owns running SyncTask to cancel the task on SyncTaskManager deletion.
  scoped_ptr<SyncTask> running_foreground_task_;

  // Owns running backgrounded SyncTask to cancel the task on SyncTaskManager
  // deletion.
  base::ScopedPtrHashMap<int64, SyncTask> running_background_tasks_;

  size_t maximum_background_task_;

  // Holds pending continuation to move task to background.
  base::Closure pending_backgrounding_task_;

  std::priority_queue<PendingTask, std::vector<PendingTask>,
                      PendingTaskComparator> pending_tasks_;
  int64 pending_task_seq_;
  int64 task_token_seq_;

  // Absence of |token_| implies a task is running. Incoming tasks should
  // wait for the task to finish in |pending_tasks_| if |token_| is null.
  // Each task must take TaskToken instance from |token_| and must hold it
  // until it finished. And the task must return the instance through
  // NotifyTaskDone when the task finished.
  scoped_ptr<SyncTaskToken> token_;

  TaskDependencyManager dependency_manager_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskManager);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
