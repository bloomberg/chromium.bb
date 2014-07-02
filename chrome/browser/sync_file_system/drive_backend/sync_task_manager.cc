// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
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
    size_t maximum_background_task,
    base::SequencedTaskRunner* task_runner)
    : client_(client),
      maximum_background_task_(maximum_background_task),
      pending_task_seq_(0),
      task_token_seq_(SyncTaskToken::kMinimumBackgroundTaskTokenID),
      task_runner_(task_runner) {
}

SyncTaskManager::~SyncTaskManager() {
  client_.reset();
  token_.reset();
}

void SyncTaskManager::Initialize(SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!token_);
  NotifyTaskDone(
      SyncTaskToken::CreateForForegroundTask(AsWeakPtr(), task_runner_),
      status);
}

void SyncTaskManager::ScheduleTask(
    const tracked_objects::Location& from_here,
    const Task& task,
    Priority priority,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  return ScheduleSyncTaskIfIdle(
      from_here,
      scoped_ptr<SyncTask>(new SyncTaskAdapter(task)),
      callback);
}

bool SyncTaskManager::ScheduleSyncTaskIfIdle(
    const tracked_objects::Location& from_here,
    scoped_ptr<SyncTask> task,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  if (token->token_id() == SyncTaskToken::kTestingTaskTokenID) {
    DCHECK(!manager);
    SyncStatusCallback callback = token->callback();
    token->clear_callback();
    callback.Run(status);
    return;
  }

  if (manager)
    manager->NotifyTaskDoneBody(token.Pass(), status);
}

// static
void SyncTaskManager::UpdateBlockingFactor(
    scoped_ptr<SyncTaskToken> current_task_token,
    scoped_ptr<BlockingFactor> blocking_factor,
    const Continuation& continuation) {
  DCHECK(current_task_token);

  SyncTaskManager* manager = current_task_token->manager();
  if (current_task_token->token_id() == SyncTaskToken::kTestingTaskTokenID) {
    DCHECK(!manager);
    continuation.Run(current_task_token.Pass());
    return;
  }

  if (!manager)
    return;

  scoped_ptr<SyncTaskToken> foreground_task_token;
  scoped_ptr<SyncTaskToken> background_task_token;
  scoped_ptr<TaskLogger::TaskLog> task_log = current_task_token->PassTaskLog();
  if (current_task_token->token_id() == SyncTaskToken::kForegroundTaskTokenID)
    foreground_task_token = current_task_token.Pass();
  else
    background_task_token = current_task_token.Pass();

  manager->UpdateBlockingFactorBody(foreground_task_token.Pass(),
                                    background_task_token.Pass(),
                                    task_log.Pass(),
                                    blocking_factor.Pass(),
                                    continuation);
}

bool SyncTaskManager::IsRunningTask(int64 token_id) const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // If the client is gone, all task should be aborted.
  if (!client_)
    return false;

  if (token_id == SyncTaskToken::kForegroundTaskTokenID)
    return true;

  return ContainsKey(running_background_tasks_, token_id);
}

void SyncTaskManager::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

void SyncTaskManager::NotifyTaskDoneBody(scoped_ptr<SyncTaskToken> token,
                                         SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(token);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  if (token->blocking_factor()) {
    dependency_manager_.Erase(token->blocking_factor());
    token->clear_blocking_factor();
  }

  if (client_) {
    if (token->has_task_log()) {
      token->FinalizeTaskLog(SyncStatusCodeToString(status));
      client_->RecordTaskLog(token->PassTaskLog());
    }
  }

  scoped_ptr<SyncTask> task;
  SyncStatusCallback callback = token->callback();
  token->clear_callback();
  if (token->token_id() == SyncTaskToken::kForegroundTaskTokenID) {
    token_ = token.Pass();
    task = running_foreground_task_.Pass();
  } else {
    task = running_background_tasks_.take_and_erase(token->token_id());
  }

  // Acquire the token to prevent a new task to jump into the queue.
  token = token_.Pass();

  bool task_used_network = false;
  if (task)
    task_used_network = task->used_network();

  if (client_)
    client_->NotifyLastOperationStatus(status, task_used_network);

  if (!callback.is_null())
    callback.Run(status);

  // Post MaybeStartNextForegroundTask rather than calling it directly to avoid
  // making the call-chaing longer.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncTaskManager::MaybeStartNextForegroundTask,
                 AsWeakPtr(), base::Passed(&token)));
}

void SyncTaskManager::UpdateBlockingFactorBody(
    scoped_ptr<SyncTaskToken> foreground_task_token,
    scoped_ptr<SyncTaskToken> background_task_token,
    scoped_ptr<TaskLogger::TaskLog> task_log,
    scoped_ptr<BlockingFactor> blocking_factor,
    const Continuation& continuation) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // Run the task directly if the parallelization is disabled.
  if (!maximum_background_task_) {
    DCHECK(foreground_task_token);
    DCHECK(!background_task_token);
    foreground_task_token->SetTaskLog(task_log.Pass());
    continuation.Run(foreground_task_token.Pass());
    return;
  }

  // Clear existing |blocking_factor| from |dependency_manager_| before
  // getting |foreground_task_token|, so that we can avoid dead lock.
  if (background_task_token && background_task_token->blocking_factor()) {
    dependency_manager_.Erase(background_task_token->blocking_factor());
    background_task_token->clear_blocking_factor();
  }

  // Try to get |foreground_task_token|.  If it's not available, wait for
  // current foreground task to finish.
  if (!foreground_task_token) {
    DCHECK(background_task_token);
    foreground_task_token = GetToken(background_task_token->location(),
                                     SyncStatusCallback());
    if (!foreground_task_token) {
      PushPendingTask(
          base::Bind(&SyncTaskManager::UpdateBlockingFactorBody,
                     AsWeakPtr(),
                     base::Passed(&foreground_task_token),
                     base::Passed(&background_task_token),
                     base::Passed(&task_log),
                     base::Passed(&blocking_factor),
                     continuation),
          PRIORITY_HIGH);
      MaybeStartNextForegroundTask(scoped_ptr<SyncTaskToken>());
      return;
    }
  }

  // Check if the task can run as a background task now.
  // If there are too many task running or any other task blocks current
  // task, wait for any other task to finish.
  bool task_number_limit_exceeded =
      !background_task_token &&
      running_background_tasks_.size() >= maximum_background_task_;
  if (task_number_limit_exceeded ||
      !dependency_manager_.Insert(blocking_factor.get())) {
    DCHECK(!running_background_tasks_.empty());
    DCHECK(pending_backgrounding_task_.is_null());

    // Wait for NotifyTaskDone to release a |blocking_factor|.
    pending_backgrounding_task_ =
        base::Bind(&SyncTaskManager::UpdateBlockingFactorBody,
                   AsWeakPtr(),
                   base::Passed(&foreground_task_token),
                   base::Passed(&background_task_token),
                   base::Passed(&task_log),
                   base::Passed(&blocking_factor),
                   continuation);
    return;
  }

  if (background_task_token) {
    background_task_token->set_blocking_factor(blocking_factor.Pass());
  } else {
    tracked_objects::Location from_here = foreground_task_token->location();
    SyncStatusCallback callback = foreground_task_token->callback();
    foreground_task_token->clear_callback();

    background_task_token =
        SyncTaskToken::CreateForBackgroundTask(
            AsWeakPtr(),
            task_runner_,
            task_token_seq_++,
            blocking_factor.Pass());
    background_task_token->UpdateTask(from_here, callback);
    running_background_tasks_.set(background_task_token->token_id(),
                                  running_foreground_task_.Pass());
  }

  token_ = foreground_task_token.Pass();
  MaybeStartNextForegroundTask(scoped_ptr<SyncTaskToken>());
  background_task_token->SetTaskLog(task_log.Pass());
  continuation.Run(background_task_token.Pass());
}

scoped_ptr<SyncTaskToken> SyncTaskManager::GetToken(
    const tracked_objects::Location& from_here,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!token_)
    return scoped_ptr<SyncTaskToken>();
  token_->UpdateTask(from_here, callback);
  return token_.Pass();
}

void SyncTaskManager::PushPendingTask(
    const base::Closure& closure, Priority priority) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  pending_tasks_.push(PendingTask(closure, priority, pending_task_seq_++));
}

void SyncTaskManager::RunTask(scoped_ptr<SyncTaskToken> token,
                              scoped_ptr<SyncTask> task) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!running_foreground_task_);

  running_foreground_task_ = task.Pass();
  running_foreground_task_->RunPreflight(token.Pass());
}

void SyncTaskManager::MaybeStartNextForegroundTask(
    scoped_ptr<SyncTaskToken> token) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (token) {
    DCHECK(!token_);
    token_ = token.Pass();
  }

  if (!pending_backgrounding_task_.is_null()) {
    base::Closure closure = pending_backgrounding_task_;
    pending_backgrounding_task_.Reset();
    closure.Run();
    return;
  }

  if (!token_)
    return;

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
