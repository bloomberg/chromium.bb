// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"

#include <algorithm>
#include <functional>

#include "base/callback.h"
#include "base/stl_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

// SyncableFileOperationRunner::Task -------------------------------------------

// static
void SyncableFileOperationRunner::Task::CancelAndDelete(
    SyncableFileOperationRunner::Task* task) {
  task->Cancel();
  delete task;
}

bool SyncableFileOperationRunner::Task::IsRunnable(
    LocalFileSyncStatus* status) const {
  for (size_t i = 0; i < target_paths().size(); ++i) {
    if (!status->IsWritable(target_paths()[i]))
      return false;
  }
  return true;
}

void SyncableFileOperationRunner::Task::Start(LocalFileSyncStatus* status) {
  for (size_t i = 0; i < target_paths().size(); ++i) {
    DCHECK(status->IsWritable(target_paths()[i]));
    status->StartWriting(target_paths()[i]);
  }
  Run();
}

// SyncableFileOperationRunner -------------------------------------------------

SyncableFileOperationRunner::SyncableFileOperationRunner(
    int64 max_inflight_tasks,
    LocalFileSyncStatus* sync_status)
    : sync_status_(sync_status),
      max_inflight_tasks_(max_inflight_tasks),
      num_inflight_tasks_(0) {
  DCHECK(CalledOnValidThread());
  sync_status_->AddObserver(this);
}

SyncableFileOperationRunner::~SyncableFileOperationRunner() {
  DCHECK(CalledOnValidThread());
  for_each(pending_tasks_.begin(), pending_tasks_.end(),
           SyncableFileOperationRunner::Task::CancelAndDelete);
}

void SyncableFileOperationRunner::OnSyncEnabled(const FileSystemURL& url) {
}

void SyncableFileOperationRunner::OnWriteEnabled(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  RunNextRunnableTask();
}

void SyncableFileOperationRunner::PostOperationTask(scoped_ptr<Task> task) {
  DCHECK(CalledOnValidThread());
  pending_tasks_.push_back(task.release());
  RunNextRunnableTask();
}

void SyncableFileOperationRunner::RunNextRunnableTask() {
  DCHECK(CalledOnValidThread());
  for (std::list<Task*>::iterator iter = pending_tasks_.begin();
       iter != pending_tasks_.end() && ShouldStartMoreTasks();) {
    if ((*iter)->IsRunnable(sync_status())) {
      ++num_inflight_tasks_;
      DCHECK_GE(num_inflight_tasks_, 1);
      scoped_ptr<Task> task(*iter);
      pending_tasks_.erase(iter++);
      task->Start(sync_status());
      continue;
    }
    ++iter;
  }
}

void SyncableFileOperationRunner::OnOperationCompleted(
    const std::vector<FileSystemURL>& target_paths) {
  --num_inflight_tasks_;
  DCHECK_GE(num_inflight_tasks_, 0);
  for (size_t i = 0; i < target_paths.size(); ++i) {
    DCHECK(sync_status()->IsWriting(target_paths[i]));
    sync_status()->EndWriting(target_paths[i]);
  }
  RunNextRunnableTask();
}

bool SyncableFileOperationRunner::ShouldStartMoreTasks() const {
  return num_inflight_tasks_ < max_inflight_tasks_;
}

}  // namespace sync_file_system
