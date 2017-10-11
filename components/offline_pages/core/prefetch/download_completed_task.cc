// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/download_completed_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

// Mirrors the OfflinePrefetchDownloadOutcomes metrics enum. Updates
// here should be reflected there and vice versa. New values should be appended
// and existing values never deleted.
enum class DownloadOutcome {
  DOWNLOAD_SUCCEEDED_ITEM_UPDATED,
  DOWNLOAD_FAILED_ITEM_UPDATED,
  DOWNLOAD_SUCCEEDED_ITEM_NOT_FOUND,
  DOWNLOAD_FAILED_ITEM_NOT_FOUND,
  COUNT  // Must always be the last element.
};

DownloadOutcome GetDownloadOutcome(bool successful_download,
                                   bool row_was_updated) {
  if (successful_download) {
    return row_was_updated ? DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_UPDATED
                           : DownloadOutcome::DOWNLOAD_SUCCEEDED_ITEM_NOT_FOUND;
  }
  return row_was_updated ? DownloadOutcome::DOWNLOAD_FAILED_ITEM_UPDATED
                         : DownloadOutcome::DOWNLOAD_FAILED_ITEM_NOT_FOUND;
}

// Updates a prefetch item after its archive was successfully downloaded.
// Returns true if the respective row was successfully updated (as normally
// expected).
bool UpdatePrefetchItemOnDownloadSuccessSync(const std::string& guid,
                                             const base::FilePath& file_path,
                                             int64_t file_size,
                                             sql::Connection* db) {
  if (!db)
    return false;

  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, file_path = ?, file_size = ?"
      " WHERE guid = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADED));
  statement.BindString(1, store_utils::ToDatabaseFilePath(file_path));
  statement.BindInt64(2, file_size);
  statement.BindString(3, guid);
  statement.BindInt(4, static_cast<int>(PrefetchItemState::DOWNLOADING));

  return statement.Run() && db->GetLastChangeCount() > 0;
}

// Updates a prefetch item after its archive failed being downloaded. Returns
// true if the respective row was successfully updated (as normally expected).
bool UpdatePrefetchItemOnDownloadErrorSync(const std::string& guid,
                                           sql::Connection* db) {
  if (!db)
    return false;

  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, error_code = ?"
      " WHERE guid = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(PrefetchItemErrorCode::DOWNLOAD_ERROR));
  statement.BindString(2, guid);
  statement.BindInt(3, static_cast<int>(PrefetchItemState::DOWNLOADING));

  return statement.Run() && db->GetLastChangeCount() > 0;
}

}  // namespace

DownloadCompletedTask::DownloadCompletedTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store,
    const PrefetchDownloadResult& download_result)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store),
      download_result_(download_result),
      weak_ptr_factory_(this) {
  DCHECK(!download_result_.download_id.empty());
}

DownloadCompletedTask::~DownloadCompletedTask() {}

void DownloadCompletedTask::Run() {
  if (download_result_.success) {
    // Reports downloaded file size in KiB (accepting values up to 100 MiB).
    UMA_HISTOGRAM_COUNTS_100000("OfflinePages.Prefetching.DownloadedFileSize",
                                download_result_.file_size / 1024);
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadSuccessSync,
                       download_result_.download_id, download_result_.file_path,
                       download_result_.file_size),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr(), true));
  } else {
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadErrorSync,
                       download_result_.download_id),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr(), false));
  }
}

void DownloadCompletedTask::OnPrefetchItemUpdated(bool successful_download,
                                                  bool row_was_updated) {
  // No further action can be done if the database fails to be updated. The
  // cleanup task should eventually kick in to clean this up.
  if (row_was_updated)
    prefetch_dispatcher_->SchedulePipelineProcessing();

  DownloadOutcome status =
      GetDownloadOutcome(successful_download, row_was_updated);
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.Prefetching.DownloadFinishedUpdate",
                            status, DownloadOutcome::COUNT);

  TaskComplete();
}

}  // namespace offline_pages
