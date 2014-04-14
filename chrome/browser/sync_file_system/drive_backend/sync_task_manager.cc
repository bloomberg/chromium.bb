// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"

using fileapi::FileSystemURL;

namespace sync_file_system {
namespace drive_backend {

namespace {

class SyncTaskAdapter : public ExclusiveTask {
 public:
  explicit SyncTaskAdapter(const SyncTaskManager::Task& task) : task_(task) {}
  virtual ~SyncTaskAdapter() {}

  virtual void RunExclusive(const SyncStatusCallback& callback) OVERRIDE {
    task_.Run(callback);
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
    base::WeakPtr<Client> client,
    size_t maximum_background_task)
    : client_(client),
      maximum_background_task_(maximum_background_task),
      pending_task_seq_(0),
      task_token_seq_(SyncTaskToken::kMinimumBackgroundTaskTokenID) {
}

SyncTaskManager::~SyncTaskManager() {
  client_.reset();
  token_.reset();
}

void SyncTaskManager::Initialize(SyncStatusCode status) {
  DCHECK(!token_);
  NotifyTaskDone(SyncTaskToken::CreateForForegroundTask(AsWeakPtr()),
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

// static
void SyncTaskManager::MoveTaskToBackground(
    scoped_ptr<SyncTaskToken> token,
    scoped_ptr<BlockingFactor> blocking_factor,
    const Continuation& continuation) {
  DCHECK(token);

  SyncTaskManager* manager = token->manager();
  if (!manager)
    return;
  manager->MoveTaskToBackgroundBody(token.Pass(), blocking_factor.Pass(),
                                    continuation);
}

bool SyncTaskManager::IsRunningTask(int64 token_id) const {
  // If the client is gone, all task should be aborted.
  if (!client_)
    return false;

  if (token_id == SyncTaskToken::kForegroundTaskTokenID)
    return true;

  return ContainsKey(running_background_task_, token_id);
}

void SyncTaskManager::NotifyTaskDoneBody(scoped_ptr<SyncTaskToken> token,
                                         SyncStatusCode status) {
  DCHECK(token);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  if (token->blocking_factor()) {
    dependency_manager_.Erase(token->blocking_factor());
    token->clear_blocking_factor();
  }

  scoped_ptr<SyncTask> task;
  SyncStatusCallback callback = token->callback();
  token->clear_callback();
  if (token->token_id() == SyncTaskToken::kForegroundTaskTokenID) {
    token_ = token.Pass();
    task = running_task_.Pass();
  } else {
    task = running_background_task_.take_and_erase(token->token_id());
  }

  bool task_used_network = false;
  if (task)
    task_used_network = task->used_network();

  if (client_)
    client_->NotifyLastOperationStatus(status, task_used_network);

  if (!callback.is_null())
    callback.Run(status);

  StartNextTask();
}

void SyncTaskManager::MoveTaskToBackgroundBody(
    scoped_ptr<SyncTaskToken> token,
    scoped_ptr<BlockingFactor> blocking_factor,
    const Continuation& continuation) {
  if (!maximum_background_task_) {
    continuation.Run(token.Pass());
    return;
  }

  if (running_background_task_.size() >= maximum_background_task_ ||
      !dependency_manager_.Insert(blocking_factor.get())) {
    DCHECK(!running_background_task_.empty());

    // Wait for NotifyTaskDone to release a |blocking_factor|.
    pending_backgrounding_task_ =
        base::Bind(&SyncTaskManager::MoveTaskToBackground,
                   base::Passed(&token), base::Passed(&blocking_factor),
                   continuation);
    return;
  }

  tracked_objects::Location from_here = token->location();
  SyncStatusCallback callback = token->callback();
  token->clear_callback();

  scoped_ptr<SyncTaskToken> background_task_token =
      SyncTaskToken::CreateForBackgroundTask(
          AsWeakPtr(), task_token_seq_++, blocking_factor.Pass());
  background_task_token->UpdateTask(from_here, callback);

  NotifyTaskBackgrounded(token.Pass(), *background_task_token);
  continuation.Run(background_task_token.Pass());
}

void SyncTaskManager::NotifyTaskBackgrounded(
    scoped_ptr<SyncTaskToken> foreground_task_token,
    const SyncTaskToken& background_task_token) {
  token_ = foreground_task_token.Pass();
  running_background_task_.set(background_task_token.token_id(),
                               running_task_.Pass());

  StartNextTask();
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
  running_task_->RunPreflight(token.Pass());
}

void SyncTaskManager::StartNextTask() {
  if (!pending_backgrounding_task_.is_null()) {
    base::Closure closure = pending_backgrounding_task_;
    pending_backgrounding_task_.Reset();
    closure.Run();
    return;
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

}  // namespace drive_backend
}  // namespace sync_file_system
