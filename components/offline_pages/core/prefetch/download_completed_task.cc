// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/download_completed_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

bool UpdatePrefetchItemOnDownloadSuccessSync(const std::string& guid,
                                             const base::FilePath& file_path,
                                             int64_t file_size,
                                             sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, file_path = ?, file_size = ?"
      " WHERE guid = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADED));
  statement.BindString(1, file_path.AsUTF8Unsafe());
  statement.BindInt64(2, file_size);
  statement.BindString(3, guid);
  statement.BindInt(4, static_cast<int>(PrefetchItemState::DOWNLOADING));

  return statement.Run();
}

bool UpdatePrefetchItemOnDownloadErrorSync(const std::string& guid,
                                           sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?, error_code = ?"
      " WHERE guid = ? AND state = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(PrefetchItemErrorCode::DOWNLOAD_ERROR));
  statement.BindString(2, guid);
  statement.BindInt(3, static_cast<int>(PrefetchItemState::DOWNLOADING));

  return statement.Run();
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
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadSuccessSync,
                       download_result_.download_id, download_result_.file_path,
                       download_result_.file_size),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    prefetch_store_->Execute(
        base::BindOnce(&UpdatePrefetchItemOnDownloadErrorSync,
                       download_result_.download_id),
        base::BindOnce(&DownloadCompletedTask::OnPrefetchItemUpdated,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DownloadCompletedTask::OnPrefetchItemUpdated(bool success) {
  // No further action can be done if the database fails to be updated. The
  // cleanup task should eventually kick in to clean this up.
  if (success)
    prefetch_dispatcher_->SchedulePipelineProcessing();

  TaskComplete();
}

}  // namespace offline_pages
