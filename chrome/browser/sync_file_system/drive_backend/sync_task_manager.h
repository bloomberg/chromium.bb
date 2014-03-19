// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace tracked_objects {
class Location;
}

namespace sync_file_system {
namespace drive_backend {

class SyncTaskToken;
struct BlockingFactor;

class SyncTaskManager
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<SyncTaskManager> {
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
  };

  explicit SyncTaskManager(base::WeakPtr<Client> client);
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

  static void NotifyTaskDone(scoped_ptr<SyncTaskToken> token,
                             SyncStatusCode status);
  static void MoveTaskToBackground(scoped_ptr<SyncTaskToken> token,
                                   scoped_ptr<BlockingFactor> blocking_factor,
                                   const Continuation& continuation);

  bool IsRunningTask(int64 task_token_id) const;

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

  // Notifies SyncTaskManager that the running task turned to a background task.
  void NotifyTaskBackgrounded(scoped_ptr<SyncTaskToken> foreground_task_token,
                              const SyncTaskToken& background_task_token);

  // Non-static version of MoveTaskToBackground.
  void MoveTaskToBackgroundBody(scoped_ptr<SyncTaskToken> token,
                                scoped_ptr<BlockingFactor> blocking_factor,
                                const Continuation& continuation);

  // Returns true if no running background task blocks |blocking_factor|.
  bool CanRunAsBackgroundTask(const BlockingFactor& blocking_factor);

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

  void StartNextTask();

  base::WeakPtr<Client> client_;

  // Owns running SyncTask to cancel the task on SyncTaskManager deletion.
  scoped_ptr<SyncTask> running_task_;

  // Owns running backgrounded SyncTask to cancel the task on SyncTaskManager
  // deletion.
  base::ScopedPtrHashMap<int64, SyncTask> running_background_task_;

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

  DISALLOW_COPY_AND_ASSIGN(SyncTaskManager);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_MANAGER_H_
