// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_database.h"

#include <limits>
#include <string>
#include <vector>

#include "base/debug/alias.h"
#include "base/file_path.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "sql/statement.h"

using content::DownloadItem;
using content::DownloadPersistentStoreInfo;

namespace history {

namespace {

static const char kSchema[] =
  "CREATE TABLE downloads ("
  "id INTEGER PRIMARY KEY,"           // SQLite-generated primary key.
  "full_path LONGVARCHAR NOT NULL,"   // Location of the download on disk.
  "url LONGVARCHAR NOT NULL,"         // URL of the downloaded file.
  "start_time INTEGER NOT NULL,"      // When the download was started.
  "received_bytes INTEGER NOT NULL,"  // Total size downloaded.
  "total_bytes INTEGER NOT NULL,"     // Total size of the download.
  "state INTEGER NOT NULL,"           // 1=complete, 2=cancelled, 4=interrupted
  "end_time INTEGER NOT NULL,"        // When the download completed.
  "opened INTEGER NOT NULL)";         // 1 if it has ever been opened else 0

#if defined(OS_POSIX)

// Binds/reads the given file path to the given column of the given statement.
void BindFilePath(sql::Statement& statement, const FilePath& path, int col) {
  statement.BindString(col, path.value());
}
FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return FilePath(statement.ColumnString(col));
}

#else

// See above.
void BindFilePath(sql::Statement& statement, const FilePath& path, int col) {
  statement.BindString16(col, path.value());
}
FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return FilePath(statement.ColumnString16(col));
}

#endif

// Key in the meta_table containing the next id to use for a new download in
// this profile.
static const char kNextDownloadId[] = "next_download_id";

}  // namespace

DownloadDatabase::DownloadDatabase()
    : owning_thread_set_(false),
      owning_thread_(0),
      next_id_(0),
      next_db_handle_(0) {
}

DownloadDatabase::~DownloadDatabase() {
}

void DownloadDatabase::CheckThread() {
  if (owning_thread_set_) {
    DCHECK(owning_thread_ == base::PlatformThread::CurrentId());
  } else {
    owning_thread_ = base::PlatformThread::CurrentId();
    owning_thread_set_ = true;
  }
}

bool DownloadDatabase::EnsureColumnExists(
    const std::string& name, const std::string& type) {
  std::string add_col = "ALTER TABLE downloads ADD COLUMN " + name + " " + type;
  return GetDB().DoesColumnExist("downloads", name.c_str()) ||
         GetDB().Execute(add_col.c_str());
}

bool DownloadDatabase::InitDownloadTable() {
  CheckThread();
  GetMetaTable().GetValue(kNextDownloadId, &next_id_);
  if (GetDB().DoesTableExist("downloads")) {
    return EnsureColumnExists("end_time", "INTEGER NOT NULL DEFAULT 0") &&
           EnsureColumnExists("opened", "INTEGER NOT NULL DEFAULT 0");
  } else {
    return GetDB().Execute(kSchema);
  }
}

bool DownloadDatabase::DropDownloadTable() {
  CheckThread();
  return GetDB().Execute("DROP TABLE downloads");
}

void DownloadDatabase::QueryDownloads(
    std::vector<DownloadPersistentStoreInfo>* results) {
  CheckThread();
  results->clear();
  if (next_db_handle_ < 1)
    next_db_handle_ = 1;
  std::set<DownloadID> db_handles;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, full_path, url, start_time, received_bytes, "
        "total_bytes, state, end_time, opened "
      "FROM downloads "
      "ORDER BY start_time"));

  while (statement.Step()) {
    DownloadPersistentStoreInfo info;
    info.db_handle = statement.ColumnInt64(0);
    info.path = ColumnFilePath(statement, 1);
    info.url = GURL(statement.ColumnString(2));
    info.start_time = base::Time::FromTimeT(statement.ColumnInt64(3));
    info.received_bytes = statement.ColumnInt64(4);
    info.total_bytes = statement.ColumnInt64(5);
    info.state = statement.ColumnInt(6);
    info.end_time = base::Time::FromTimeT(statement.ColumnInt64(7));
    info.opened = statement.ColumnInt(8) != 0;
    results->push_back(info);
    if (info.db_handle >= next_db_handle_)
      next_db_handle_ = info.db_handle + 1;
    if (!db_handles.insert(info.db_handle).second) {
      // info.db_handle was already in db_handles. The database is corrupt.
      base::debug::Alias(&info.db_handle);
      DCHECK(false);
    }
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadPersistentStoreInfo& data) {
  CheckThread();
  DCHECK(data.db_handle > 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads "
      "SET received_bytes=?, state=?, end_time=?, opened=? WHERE id=?"));
  statement.BindInt64(0, data.received_bytes);
  statement.BindInt(1, data.state);
  statement.BindInt64(2, data.end_time.ToTimeT());
  statement.BindInt(3, (data.opened ? 1 : 0));
  statement.BindInt64(4, data.db_handle);

  return statement.Run();
}

bool DownloadDatabase::UpdateDownloadPath(const FilePath& path,
                                          DownloadID db_handle) {
  CheckThread();
  DCHECK(db_handle > 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET full_path=? WHERE id=?"));
  BindFilePath(statement, path, 0);
  statement.BindInt64(1, db_handle);

  return statement.Run();
}

bool DownloadDatabase::CleanUpInProgressEntries() {
  CheckThread();
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET state=? WHERE state=?"));
  statement.BindInt(0, DownloadItem::CANCELLED);
  statement.BindInt(1, DownloadItem::IN_PROGRESS);

  return statement.Run();
}

int64 DownloadDatabase::CreateDownload(
    const DownloadPersistentStoreInfo& info) {
  CheckThread();

  if (next_db_handle_ == 0) {
    // This is unlikely. All current known tests and users already call
    // QueryDownloads() before CreateDownload().
    std::vector<DownloadPersistentStoreInfo> results;
    QueryDownloads(&results);
    CHECK_NE(0, next_db_handle_);
  }

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO downloads "
      "(id, full_path, url, start_time, received_bytes, total_bytes, state, "
      "end_time, opened) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  int db_handle = next_db_handle_++;

  statement.BindInt64(0, db_handle);
  BindFilePath(statement, info.path, 1);
  statement.BindString(2, info.url.spec());
  statement.BindInt64(3, info.start_time.ToTimeT());
  statement.BindInt64(4, info.received_bytes);
  statement.BindInt64(5, info.total_bytes);
  statement.BindInt(6, info.state);
  statement.BindInt64(7, info.end_time.ToTimeT());
  statement.BindInt(8, info.opened ? 1 : 0);

  if (statement.Run()) {
    // TODO(benjhayden) if(info.id>next_id_){setvalue;next_id_=info.id;}
    GetMetaTable().SetValue(kNextDownloadId, ++next_id_);

    return db_handle;
  }
  return 0;
}

void DownloadDatabase::RemoveDownload(DownloadID db_handle) {
  CheckThread();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  statement.BindInt64(0, db_handle);

  statement.Run();
}

bool DownloadDatabase::RemoveDownloadsBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  CheckThread();
  time_t start_time = delete_begin.ToTimeT();
  time_t end_time = delete_end.ToTimeT();

  int num_downloads_deleted = -1;
  {
    sql::Statement count(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "SELECT count(*) FROM downloads WHERE start_time >= ? "
        "AND start_time < ? AND (State = ? OR State = ? OR State = ?)"));
    count.BindInt64(0, start_time);
    count.BindInt64(
        1,
        end_time ? end_time : std::numeric_limits<int64>::max());
    count.BindInt(2, DownloadItem::COMPLETE);
    count.BindInt(3, DownloadItem::CANCELLED);
    count.BindInt(4, DownloadItem::INTERRUPTED);
    if (count.Step())
      num_downloads_deleted = count.ColumnInt(0);
  }


  bool success = false;
  base::TimeTicks started_removing = base::TimeTicks::Now();
  {
    // This does not use an index. We currently aren't likely to have enough
    // downloads where an index by time will give us a lot of benefit.
    sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "DELETE FROM downloads WHERE start_time >= ? AND start_time < ? "
        "AND (State = ? OR State = ? OR State = ?)"));
    statement.BindInt64(0, start_time);
    statement.BindInt64(
        1,
        end_time ? end_time : std::numeric_limits<int64>::max());
    statement.BindInt(2, DownloadItem::COMPLETE);
    statement.BindInt(3, DownloadItem::CANCELLED);
    statement.BindInt(4, DownloadItem::INTERRUPTED);

    success = statement.Run();
  }

  base::TimeTicks finished_removing = base::TimeTicks::Now();

  if (num_downloads_deleted >= 0) {
    UMA_HISTOGRAM_COUNTS("Download.DatabaseRemoveDownloadsCount",
                         num_downloads_deleted);
    base::TimeDelta micros = (1000 * (finished_removing - started_removing));
    UMA_HISTOGRAM_TIMES("Download.DatabaseRemoveDownloadsTime", micros);
    if (num_downloads_deleted > 0) {
      UMA_HISTOGRAM_TIMES("Download.DatabaseRemoveDownloadsTimePerRecord",
                          (1000 * micros) / num_downloads_deleted);
    }
  }

  return success;
}

}  // namespace history
