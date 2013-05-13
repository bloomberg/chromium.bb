// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_task_manager.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

class DriveFileSyncTaskManager::TaskToken {
 public:
  explicit TaskToken(const base::WeakPtr<DriveFileSyncTaskManager>& manager)
      : manager_(manager) {
  }

  void UpdateTask(const tracked_objects::Location& location) {
    location_ = location;
    DVLOG(2) << "Token updated: " << location_.ToString();
  }

  const tracked_objects::Location& location() const { return location_; }

  ~TaskToken() {
    // All task on DriveFileSyncService must hold TaskToken instance to ensure
    // no other tasks are running. Also, as soon as a task finishes to work,
    // it must return the token to DriveFileSyncService.
    // Destroying a token with valid |sync_service_| indicates the token was
    // dropped by a task without returning.
    if (manager_ && manager_->service_.get()) {
      NOTREACHED() << "Unexpected TaskToken deletion from: "
                   << location_.ToString();

      // Reinitializes the token.
      manager_->NotifyTaskDone(
          make_scoped_ptr(new TaskToken(manager_)),
          SyncStatusCallback(),
          SYNC_STATUS_OK);
    }
  }

 private:
  base::WeakPtr<DriveFileSyncTaskManager> manager_;
  tracked_objects::Location location_;

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

DriveFileSyncTaskManager::DriveFileSyncTaskManager(
    base::WeakPtr<DriveFileSyncService> service)
    : service_(service),
      last_operation_status_(SYNC_STATUS_OK),
      last_gdata_error_(google_apis::HTTP_SUCCESS) {
}

DriveFileSyncTaskManager::~DriveFileSyncTaskManager() {
  service_.reset();
  token_.reset();
}

void DriveFileSyncTaskManager::Initialize(SyncStatusCode status) {
  DCHECK(!token_);
  NotifyTaskDone(make_scoped_ptr(new TaskToken(AsWeakPtr())),
                 SyncStatusCallback(),
                 status);
}

void DriveFileSyncTaskManager::ScheduleTask(
    const Task& task,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncTaskManager::ScheduleTask, AsWeakPtr(), task, callback));
    return;
  }
  task.Run(CreateCompletionCallback(token.Pass(), callback));
}

void DriveFileSyncTaskManager::ScheduleTaskIfIdle(const Task& task) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token)
    return;
  task.Run(CreateCompletionCallback(token.Pass(), SyncStatusCallback()));
}

void DriveFileSyncTaskManager::NotifyLastDriveError(
    google_apis::GDataErrorCode error) {
  last_gdata_error_ = error;
}

void DriveFileSyncTaskManager::NotifyTaskDone(
    scoped_ptr<TaskToken> token,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DCHECK(token);
  last_operation_status_ = status;
  token_ = token.Pass();
  TRACE_EVENT_ASYNC_END0("Sync FileSystem", "GetToken", this);

  DVLOG(3) << "NotifyTaskDone: " << "finished with status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " " << token_->location().ToString();

  service_->NotifyLastOperationStatus(last_operation_status_,
                                      last_gdata_error_);

  if (!pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.front();
    pending_tasks_.pop_front();
    closure.Run();
    return;
  }

  if (!callback.is_null())
    callback.Run(status);

  service_->MaybeScheduleNextTask();
}

scoped_ptr<DriveFileSyncTaskManager::TaskToken>
DriveFileSyncTaskManager::GetToken(
    const tracked_objects::Location& from_here) {
  if (!token_)
    return scoped_ptr<TaskToken>();
  TRACE_EVENT_ASYNC_BEGIN1("Sync FileSystem", "GetToken", this,
                           "where", from_here.ToString());
  token_->UpdateTask(from_here);
  return token_.Pass();
}

SyncStatusCallback DriveFileSyncTaskManager::CreateCompletionCallback(
    scoped_ptr<TaskToken> token,
    const SyncStatusCallback& callback) {
  DCHECK(token);
  return base::Bind(&DriveFileSyncTaskManager::NotifyTaskDone,
                    AsWeakPtr(), base::Passed(&token), callback);
}

}  // namespace sync_file_system
