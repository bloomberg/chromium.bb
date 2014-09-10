// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"

namespace sync_file_system {
namespace drive_backend {

ListChangesTask::ListChangesTask(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      weak_ptr_factory_(this) {
}

ListChangesTask::~ListChangesTask() {
}

void ListChangesTask::RunPreflight(scoped_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("List Changes");

  if (!IsContextReady()) {
    token->RecordLog("Failed to get required service.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  SyncTaskManager::UpdateTaskBlocker(
      token.Pass(),
      scoped_ptr<TaskBlocker>(new TaskBlocker),
      base::Bind(&ListChangesTask::StartListing,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ListChangesTask::StartListing(scoped_ptr<SyncTaskToken> token) {
  drive_service()->GetChangeList(
      metadata_database()->GetLargestFetchedChangeID() + 1,
      base::Bind(&ListChangesTask::DidListChanges,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void ListChangesTask::DidListChanges(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ChangeList> change_list) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    token->RecordLog("Failed to fetch change list.");
    SyncTaskManager::NotifyTaskDone(
        token.Pass(), SYNC_STATUS_NETWORK_ERROR);
    return;
  }

  if (!change_list) {
    NOTREACHED();
    token->RecordLog("Got invalid change list.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  std::vector<google_apis::ChangeResource*> changes;
  change_list->mutable_items()->release(&changes);

  change_list_.reserve(change_list_.size() + changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    change_list_.push_back(changes[i]);

  if (!change_list->next_link().is_empty()) {
    drive_service()->GetRemainingChangeList(
        change_list->next_link(),
        base::Bind(
            &ListChangesTask::DidListChanges,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&token)));
    return;
  }

  if (change_list_.empty()) {
    token->RecordLog("Got no change.");
    SyncTaskManager::NotifyTaskDone(
        token.Pass(), SYNC_STATUS_NO_CHANGE_TO_SYNC);
    return;
  }

  scoped_ptr<TaskBlocker> task_blocker(new TaskBlocker);
  task_blocker->exclusive = true;
  SyncTaskManager::UpdateTaskBlocker(
      token.Pass(),
      task_blocker.Pass(),
      base::Bind(&ListChangesTask::CheckInChangeList,
                 weak_ptr_factory_.GetWeakPtr(),
                 change_list->largest_change_id()));
}

void ListChangesTask::CheckInChangeList(int64 largest_change_id,
                                        scoped_ptr<SyncTaskToken> token) {
  token->RecordLog(base::StringPrintf(
      "Got %" PRIuS " changes, updating MetadataDatabase.",
      change_list_.size()));

  DCHECK(file_ids_.empty());
  file_ids_.reserve(change_list_.size());
  for (size_t i = 0; i < change_list_.size(); ++i)
    file_ids_.push_back(change_list_[i]->file_id());

  SyncStatusCode status =
      metadata_database()->UpdateByChangeList(
          largest_change_id, change_list_.Pass());
  if (status != SYNC_STATUS_OK) {
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  status = metadata_database()->SweepDirtyTrackers(file_ids_);
  SyncTaskManager::NotifyTaskDone(token.Pass(), status);
}

bool ListChangesTask::IsContextReady() {
  return sync_context_->GetMetadataDatabase() &&
      sync_context_->GetDriveService();
}

MetadataDatabase* ListChangesTask::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* ListChangesTask::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

}  // namespace drive_backend
}  // namespace sync_file_system
