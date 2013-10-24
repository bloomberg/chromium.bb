// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

scoped_ptr<google_apis::ChangeResource> ConvertResourceEntryToChangeResource(
    const google_apis::ResourceEntry& entry) {
  scoped_ptr<google_apis::ChangeResource> out(new google_apis::ChangeResource);
  out->set_file_id(entry.resource_id());
  if (!entry.deleted())
    out->set_file(drive::util::ConvertResourceEntryToFileResource(entry));
  out->set_change_id(entry.changestamp());
  out->set_deleted(entry.deleted());

  return out.Pass();
}

}  // namespace

ListChangesTask::ListChangesTask(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      weak_ptr_factory_(this) {
}

ListChangesTask::~ListChangesTask() {
}

void ListChangesTask::Run(const SyncStatusCallback& callback) {
  if (!metadata_database() || !drive_service()) {
    util::Log(logging::LOG_ERROR, FROM_HERE, "Failed to get required sercive.");
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_FAILED));
    return;
  }

  drive_service()->GetChangeList(
      metadata_database()->GetLargestChangeID() + 1,
      base::Bind(&ListChangesTask::DidListChanges,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ListChangesTask::DidListChanges(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  if (error != google_apis::HTTP_SUCCESS) {
    util::Log(logging::LOG_ERROR, FROM_HERE, "Failed to fetch change list.");
    callback.Run(SYNC_STATUS_NETWORK_ERROR);
    return;
  }

  change_list_.reserve(change_list_.size() + resource_list->entries().size());
  for (size_t i = 0; i < resource_list->entries().size(); ++i) {
    change_list_.push_back(ConvertResourceEntryToChangeResource(
        *resource_list->entries()[i]).release());
  }

  // TODO(tzik): http://crbug.com/310964
  // This may take long time to run in single task.  Run this as a background
  // task.
  GURL next_feed;
  if (resource_list->GetNextFeedURL(&next_feed)) {
    drive_service()->GetRemainingChangeList(
        next_feed,
        base::Bind(
            &ListChangesTask::DidListChanges,
            weak_ptr_factory_.GetWeakPtr(),
            callback));
    return;
  }

  metadata_database()->UpdateByChangeList(
      resource_list->largest_changestamp(),
      change_list_.Pass(), callback);
}

MetadataDatabase* ListChangesTask::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* ListChangesTask::drive_service() {
  return sync_context_->GetDriveService();
}

}  // namespace drive_backend
}  // namespace sync_file_system
