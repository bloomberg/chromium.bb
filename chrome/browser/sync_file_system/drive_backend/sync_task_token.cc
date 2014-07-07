// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

namespace sync_file_system {
namespace drive_backend {

const int64 SyncTaskToken::kTestingTaskTokenID = -1;
const int64 SyncTaskToken::kForegroundTaskTokenID = 0;
const int64 SyncTaskToken::kMinimumBackgroundTaskTokenID = 1;

// static
scoped_ptr<SyncTaskToken> SyncTaskToken::CreateForTesting(
    const SyncStatusCallback& callback) {
  return make_scoped_ptr(new SyncTaskToken(
      base::WeakPtr<SyncTaskManager>(),
      base::ThreadTaskRunnerHandle::Get(),
      kTestingTaskTokenID,
      scoped_ptr<BlockingFactor>(),
      callback));
}

// static
scoped_ptr<SyncTaskToken> SyncTaskToken::CreateForForegroundTask(
    const base::WeakPtr<SyncTaskManager>& manager,
    base::SequencedTaskRunner* task_runner) {
  return make_scoped_ptr(new SyncTaskToken(
      manager,
      task_runner,
      kForegroundTaskTokenID,
      scoped_ptr<BlockingFactor>(),
      SyncStatusCallback()));
}

// static
scoped_ptr<SyncTaskToken> SyncTaskToken::CreateForBackgroundTask(
    const base::WeakPtr<SyncTaskManager>& manager,
    base::SequencedTaskRunner* task_runner,
    int64 token_id,
    scoped_ptr<BlockingFactor> blocking_factor) {
  return make_scoped_ptr(new SyncTaskToken(
      manager,
      task_runner,
      token_id,
      blocking_factor.Pass(),
      SyncStatusCallback()));
}

void SyncTaskToken::UpdateTask(const tracked_objects::Location& location,
                               const SyncStatusCallback& callback) {
  DCHECK(callback_.is_null());
  location_ = location;
  callback_ = callback;
  DVLOG(2) << "Token updated: " << location_.ToString();
}

SyncTaskToken::~SyncTaskToken() {
  // All task on Client must hold TaskToken instance to ensure
  // no other tasks are running. Also, as soon as a task finishes to work,
  // it must return the token to TaskManager.
  // Destroying a token with valid |client| indicates the token was
  // dropped by a task without returning.
  if (task_runner_ && task_runner_->RunsTasksOnCurrentThread() &&
      manager_ && manager_->IsRunningTask(token_id_)) {
    NOTREACHED()
        << "Unexpected TaskToken deletion from: " << location_.ToString();

    // Reinitializes the token.
    SyncTaskManager::NotifyTaskDone(
        make_scoped_ptr(new SyncTaskToken(
            manager_, task_runner_, token_id_, blocking_factor_.Pass(),
            SyncStatusCallback())),
        SYNC_STATUS_OK);
  }
}

// static
SyncStatusCallback SyncTaskToken::WrapToCallback(
    scoped_ptr<SyncTaskToken> token) {
  return base::Bind(&SyncTaskManager::NotifyTaskDone, base::Passed(&token));
}

void SyncTaskToken::set_blocking_factor(
    scoped_ptr<BlockingFactor> blocking_factor) {
  blocking_factor_ = blocking_factor.Pass();
}

const BlockingFactor* SyncTaskToken::blocking_factor() const {
  return blocking_factor_.get();
}

void SyncTaskToken::clear_blocking_factor() {
  blocking_factor_.reset();
}

void SyncTaskToken::InitializeTaskLog(const std::string& task_description) {
  task_log_.reset(new TaskLogger::TaskLog);
  task_log_->start_time = base::TimeTicks::Now();
  task_log_->task_description = task_description;

  TRACE_EVENT_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("SyncFileSystem"),
      "SyncTask", task_log_->log_id,
      "task_description", task_description);
}

void SyncTaskToken::FinalizeTaskLog(const std::string& result_description) {
  TRACE_EVENT_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("SyncFileSystem"),
      "SyncTask", task_log_->log_id,
      "result_description", result_description);

  DCHECK(task_log_);
  task_log_->result_description = result_description;
  task_log_->end_time = base::TimeTicks::Now();
}

void SyncTaskToken::RecordLog(const std::string& message) {
  DCHECK(task_log_);
  task_log_->details.push_back(message);
}

void SyncTaskToken::SetTaskLog(scoped_ptr<TaskLogger::TaskLog> task_log) {
  task_log_ = task_log.Pass();
}

scoped_ptr<TaskLogger::TaskLog> SyncTaskToken::PassTaskLog() {
  return task_log_.Pass();
}

SyncTaskToken::SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager,
                             base::SequencedTaskRunner* task_runner,
                             int64 token_id,
                             scoped_ptr<BlockingFactor> blocking_factor,
                             const SyncStatusCallback& callback)
    : manager_(manager),
      task_runner_(task_runner),
      token_id_(token_id),
      callback_(callback),
      blocking_factor_(blocking_factor.Pass()) {
}

}  // namespace drive_backend
}  // namespace sync_file_system
