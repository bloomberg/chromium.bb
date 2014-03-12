// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_task_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_task_token.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

class SyncTaskAdapter : public SyncTask {
 public:
  explicit SyncTaskAdapter(const SyncTaskManager::Task& task) : task_(task) {}
  virtual ~SyncTaskAdapter() {}

  virtual void Run(scoped_ptr<SyncTaskToken> token) OVERRIDE {
    task_.Run(SyncTaskToken::WrapToCallback(token.Pass()));
  }

 private:
  SyncTaskManager::Task task_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskAdapter);
};

}  // namespace

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
  NotifyTaskDone(make_scoped_ptr(new SyncTaskToken(AsWeakPtr())),
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
  scoped_ptr<SyncTaskToken> token(GetToken(from_here, callback));
  if (!token) {
    PushPendingTask(
        base::Bind(&SyncTaskManager::ScheduleSyncTask, AsWeakPtr(), from_here,
                   base::Passed(&task), priority, callback),
        priority);
    return;
  }
  RunTask(token.Pass(), task.Pass());
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
  scoped_ptr<SyncTaskToken> token(GetToken(from_here, callback));
  if (!token)
    return false;
  RunTask(token.Pass(), task.Pass());
  return true;
}

// static
void SyncTaskManager::NotifyTaskDone(scoped_ptr<SyncTaskToken> token,
                                     SyncStatusCode status) {
  DCHECK(token);

  SyncTaskManager* manager = token->manager();
  if (manager)
    manager->NotifyTaskDoneBody(token.Pass(), status);
}

bool SyncTaskManager::HasClient() const {
  return !!client_;
}

void SyncTaskManager::NotifyTaskDoneBody(scoped_ptr<SyncTaskToken> token,
                                         SyncStatusCode status) {
  DCHECK(token);

  token_ = token.Pass();
  scoped_ptr<SyncTask> task = running_task_.Pass();

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  bool task_used_network = false;
  if (task)
    task_used_network = task->used_network();

  if (client_)
    client_->NotifyLastOperationStatus(status, task_used_network);

  if (!token_->callback().is_null()) {
    SyncStatusCallback callback = token_->callback();
    token_->clear_callback();
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

scoped_ptr<SyncTaskToken> SyncTaskManager::GetToken(
    const tracked_objects::Location& from_here,
    const SyncStatusCallback& callback) {
  if (!token_)
    return scoped_ptr<SyncTaskToken>();
  token_->UpdateTask(from_here, callback);
  return token_.Pass();
}

void SyncTaskManager::PushPendingTask(
    const base::Closure& closure, Priority priority) {
  pending_tasks_.push(PendingTask(closure, priority, pending_task_seq_++));
}

void SyncTaskManager::RunTask(scoped_ptr<SyncTaskToken> token,
                              scoped_ptr<SyncTask> task) {
  DCHECK(!running_task_);
  running_task_ = task.Pass();
  running_task_->Run(token.Pass());
}

}  // namespace sync_file_system
