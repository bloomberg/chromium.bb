// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/metrics_finalization_task.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/offline_time_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

// In-memory representation of URL metadata fetched from SQLite storage.
struct PrefetchItemStats {
  PrefetchItemStats(int64_t offline_id,
                    int generate_bundle_attempts,
                    int get_operation_attempts,
                    int download_initiation_attempts,
                    int64_t archive_body_length,
                    base::Time creation_time,
                    PrefetchItemErrorCode error_code,
                    int64_t file_size)
      : offline_id(offline_id),
        generate_bundle_attempts(generate_bundle_attempts),
        get_operation_attempts(get_operation_attempts),
        download_initiation_attempts(download_initiation_attempts),
        archive_body_length(archive_body_length),
        creation_time(creation_time),
        error_code(error_code),
        file_size(file_size) {}

  int64_t offline_id;
  int generate_bundle_attempts;
  int get_operation_attempts;
  int download_initiation_attempts;
  int64_t archive_body_length;
  base::Time creation_time;
  PrefetchItemErrorCode error_code;
  int64_t file_size;
};

std::unique_ptr<std::vector<PrefetchItemStats>> FetchUrlsSync(
    sql::Connection* db) {
  static const char kSql[] = R"(
  SELECT offline_id, generate_bundle_attempts, get_operation_attempts,
    download_initiation_attempts, archive_body_length, creation_time,
    error_code, file_size
  FROM prefetch_items
  WHERE state = ?
)";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));

  auto urls = base::MakeUnique<std::vector<PrefetchItemStats>>();
  while (statement.Step()) {
    PrefetchItemStats stats(
        statement.ColumnInt64(0),  // offline_id
        statement.ColumnInt(1),    // generate_bundle_attempts
        statement.ColumnInt(2),    // get_operation_attempts
        statement.ColumnInt(3),    // download_initiation_attempts
        statement.ColumnInt64(4),  // archive_body_length
        FromDatabaseTime(statement.ColumnInt64(5)),  // creation_time
        static_cast<PrefetchItemErrorCode>(
            statement.ColumnInt(6)),  // error_code
        statement.ColumnInt64(7)      // file_size
        );

    urls->push_back(stats);
  }

  return urls;
}

bool MarkUrlAsZombie(sql::Connection* db,
                     base::Time freshness_time,
                     int64_t offline_id) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, freshness_time = ? WHERE "
      "offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::ZOMBIE));
  statement.BindInt(1, ToDatabaseTime(freshness_time));
  statement.BindInt64(2, offline_id);
  return statement.Run();
}

bool SelectUrlsToPrefetchSync(sql::Connection* db) {
  if (!db)
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  auto urls = FetchUrlsSync(db);

  base::Time now = base::Time::Now();
  for (const auto& url : *urls) {
    MarkUrlAsZombie(db, now, url.offline_id);
  }

  if (transaction.Commit()) {
    // TODO(dewittj): Report interesting UMA metrics for each prefetch item.
    for (const auto& url : *urls) {
      DVLOG(1) << "Finalized prefetch item: (" << url.offline_id << ", "
               << url.generate_bundle_attempts << ", "
               << url.get_operation_attempts << ", "
               << url.download_initiation_attempts << ", "
               << url.archive_body_length << ", " << url.creation_time << ", "
               << static_cast<int>(url.error_code) << ", " << url.file_size
               << ")";
    }

    return true;
  }

  return false;
}

}  // namespace

MetricsFinalizationTask::MetricsFinalizationTask(PrefetchStore* prefetch_store)
    : prefetch_store_(prefetch_store), weak_factory_(this) {}

MetricsFinalizationTask::~MetricsFinalizationTask() {}

void MetricsFinalizationTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&SelectUrlsToPrefetchSync),
      base::BindOnce(&MetricsFinalizationTask::MetricsFinalized,
                     weak_factory_.GetWeakPtr()));
}

void MetricsFinalizationTask::MetricsFinalized(bool result) {
  TaskComplete();
}

}  // namespace offline_pages
