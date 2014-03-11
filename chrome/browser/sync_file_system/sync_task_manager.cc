// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_task_manager.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

class SyncTaskAdapter : public SyncTask {
 public:
  explicit SyncTaskAdapter(const SyncTaskManager::Task& task) : task_(task) {}
  virtual ~SyncTaskAdapter() {}

  virtual void Run(const SyncStatusCallback& callback) OVERRIDE {
    task_.Run(callback);
  }

 private:
  SyncTaskManager::Task task_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskAdapter);
};

}  // namespace

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
    const tracked_objects::Location& from_here,
    const Task& task,
    Priority priority,
    const SyncStatusCallback& callback) {
  ScheduleSyncTask(from_here,
                   scoped_ptr<SyncTask>(new SyncTaskAdapter(task)),
                   priority,
                   callback);
}

void SyncTaskManager::ScheduleSyncTask(
    const tracked_objects::Location& from_here,
    scoped_ptr<SyncTask> task,
    Priority priority,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(from_here));
  if (!token) {
    PushPendingTask(
        base::Bind(&SyncTaskManager::ScheduleSyncTask, AsWeakPtr(), from_here,
                   base::Passed(&task), priority, callback),
        priority);
    return;
  }
  RunTask(token.Pass(), task.Pass(), callback);
}

bool SyncTaskManager::ScheduleTaskIfIdle(
        const tracked_objects::Location& from_here,
        const Task& task,
        const SyncStatusCallback& callback) {
  return ScheduleSyncTaskIfIdle(
      from_here,
      scoped_ptr<SyncTask>(new SyncTaskAdapter(task)),
      callback);
}

bool SyncTaskManager::ScheduleSyncTaskIfIdle(
    const tracked_objects::Location& from_here,
    scoped_ptr<SyncTask> task,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(from_here));
  if (!token)
    return false;
  RunTask(token.Pass(), task.Pass(), callback);
  return true;
}

void SyncTaskManager::NotifyTaskDone(
    scoped_ptr<TaskToken> token,
    SyncStatusCode status) {
  DCHECK(token);
  token_ = token.Pass();
  scoped_ptr<SyncTask> task = running_task_.Pass();
  TRACE_EVENT_ASYNC_END0("Sync FileSystem", "GetToken", this);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  bool task_used_network = false;
  if (task)
    task_used_network = task->used_network();

  if (client_)
    client_->NotifyLastOperationStatus(status, task_used_network);

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

void SyncTaskManager::PushPendingTask(
    const base::Closure& closure, Priority priority) {
  pending_tasks_.push(PendingTask(closure, priority, pending_task_seq_++));
}

void SyncTaskManager::RunTask(scoped_ptr<TaskToken> token,
                              scoped_ptr<SyncTask> task,
                              const SyncStatusCallback& callback) {
  DCHECK(!running_task_);
  DCHECK(current_callback_.is_null());
  running_task_ = task.Pass();
  current_callback_ = callback;
  running_task_->Run(
      base::Bind(&SyncTaskManager::NotifyTaskDone, AsWeakPtr(),
                 base::Passed(&token)));
}

}  // namespace sync_file_system
