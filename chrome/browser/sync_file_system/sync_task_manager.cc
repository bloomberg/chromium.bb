// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_task_manager.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

class SyncTaskManager::TaskToken {
 public:
  explicit TaskToken(const base::WeakPtr<SyncTaskManager>& manager)
      : manager_(manager) {
  }

  void UpdateTask(const tracked_objects::Location& location) {
    location_ = location;
    DVLOG(2) << "Token updated: " << location_.ToString();
  }

  const tracked_objects::Location& location() const { return location_; }

  ~TaskToken() {
    // All task on Client must hold TaskToken instance to ensure
    // no other tasks are running. Also, as soon as a task finishes to work,
    // it must return the token to TaskManager.
    // Destroying a token with valid |client| indicates the token was
    // dropped by a task without returning.
    if (manager_.get() && manager_->client_.get()) {
      NOTREACHED()
          << "Unexpected TaskToken deletion from: " << location_.ToString();

      // Reinitializes the token.
      manager_->NotifyTaskDone(
          make_scoped_ptr(new TaskToken(manager_)),
          SYNC_STATUS_OK);
    }
  }

 private:
  base::WeakPtr<SyncTaskManager> manager_;
  tracked_objects::Location location_;

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

SyncTaskManager::PendingTask::PendingTask() {}

SyncTaskManager::PendingTask::PendingTask(
    const base::Closure& task, Priority pri, int seq)
    : task(task), priority(pri), seq(seq) {}

SyncTaskManager::PendingTask::~PendingTask() {}

bool SyncTaskManager::PendingTaskComparator::operator()(
    const PendingTask& left,
    const PendingTask& right) const {
  if (left.priority != right.priority)
    return left.priority < right.priority;
  return left.seq > right.seq;
}

SyncTaskManager::SyncTaskManager(
    base::WeakPtr<Client> client)
    : client_(client),
      last_operation_status_(SYNC_STATUS_OK),
      pending_task_seq_(0) {
}

SyncTaskManager::~SyncTaskManager() {
  client_.reset();
  token_.reset();
}

void SyncTaskManager::Initialize(SyncStatusCode status) {
  DCHECK(!token_);
  NotifyTaskDone(make_scoped_ptr(new TaskToken(AsWeakPtr())),
                 status);
}

void SyncTaskManager::ScheduleTask(
    const Task& task,
    const SyncStatusCallback& callback) {
  ScheduleTaskAtPriority(task, PRIORITY_MED, callback);
}

void SyncTaskManager::ScheduleSyncTask(
    scoped_ptr<SyncTask> task,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    PushPendingTask(
        base::Bind(&SyncTaskManager::ScheduleSyncTask,
                   AsWeakPtr(), base::Passed(&task), callback),
        PRIORITY_MED);
    return;
  }
  DCHECK(!running_task_);
  running_task_ = task.Pass();
  running_task_->Run(CreateCompletionCallback(token.Pass(), callback));
}

void SyncTaskManager::ScheduleTaskAtPriority(
    const Task& task,
    Priority priority,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    PushPendingTask(
        base::Bind(&SyncTaskManager::ScheduleTask, AsWeakPtr(),
                   task, callback),
        priority);
    return;
  }
  task.Run(CreateCompletionCallback(token.Pass(), callback));
}

bool SyncTaskManager::ScheduleTaskIfIdle(const Task& task) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token)
    return false;
  task.Run(CreateCompletionCallback(token.Pass(), SyncStatusCallback()));
  return true;
}

bool SyncTaskManager::ScheduleSyncTaskIfIdle(scoped_ptr<SyncTask> task) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token)
    return false;
  DCHECK(!running_task_);
  running_task_ = task.Pass();
  running_task_->Run(CreateCompletionCallback(token.Pass(),
                                              SyncStatusCallback()));
  return true;
}

void SyncTaskManager::NotifyTaskDone(
    scoped_ptr<TaskToken> token,
    SyncStatusCode status) {
  DCHECK(token);
  last_operation_status_ = status;
  token_ = token.Pass();
  scoped_ptr<SyncTask> task = running_task_.Pass();
  TRACE_EVENT_ASYNC_END0("Sync FileSystem", "GetToken", this);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  if (client_)
    client_->NotifyLastOperationStatus(last_operation_status_);

  if (!current_callback_.is_null()) {
    SyncStatusCallback callback = current_callback_;
    current_callback_.Reset();
    callback.Run(status);
  }

  if (!pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.top().task;
    pending_tasks_.pop();
    closure.Run();
    return;
  }

  if (client_)
    client_->MaybeScheduleNextTask();
}

scoped_ptr<SyncTaskManager::TaskToken> SyncTaskManager::GetToken(
    const tracked_objects::Location& from_here) {
  if (!token_)
    return scoped_ptr<TaskToken>();
  TRACE_EVENT_ASYNC_BEGIN1("Sync FileSystem", "GetToken", this,
                           "where", from_here.ToString());
  token_->UpdateTask(from_here);
  return token_.Pass();
}

SyncStatusCallback SyncTaskManager::CreateCompletionCallback(
    scoped_ptr<TaskToken> token,
    const SyncStatusCallback& callback) {
  DCHECK(token);
  DCHECK(current_callback_.is_null());
  current_callback_ = callback;
  return base::Bind(&SyncTaskManager::NotifyTaskDone,
                    AsWeakPtr(), base::Passed(&token));
}

void SyncTaskManager::PushPendingTask(
    const base::Closure& closure, Priority priority) {
  pending_tasks_.push(PendingTask(closure, priority, pending_task_seq_++));
}

}  // namespace sync_file_system
