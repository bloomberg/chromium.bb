// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_database.h"

#include <limits>
#include <vector>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "sql/statement.h"

// Download schema:
//
//   id             SQLite-generated primary key.
//   full_path      Location of the download on disk.
//   url            URL of the downloaded file.
//   start_time     When the download was started.
//   received_bytes Total size downloaded.
//   total_bytes    Total size of the download.
//   state          Identifies if this download is completed or not. Not used
//                  directly by the history system. See DownloadItem's
//                  DownloadState for where this is used.

namespace history {

namespace {

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
  statement.BindString(col, UTF16ToUTF8(path.value()));
}
FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return FilePath(UTF8ToUTF16(statement.ColumnString(col)));
}

#endif

}  // namespace

DownloadDatabase::DownloadDatabase()
    : owning_thread_set_(false) {
}

DownloadDatabase::~DownloadDatabase() {
}

bool DownloadDatabase::InitDownloadTable() {
  if (!GetDB().DoesTableExist("downloads")) {
    if (!GetDB().Execute(
        "CREATE TABLE downloads ("
        "id INTEGER PRIMARY KEY,"
        "full_path LONGVARCHAR NOT NULL,"
        "url LONGVARCHAR NOT NULL,"
        "start_time INTEGER NOT NULL,"
        "received_bytes INTEGER NOT NULL,"
        "total_bytes INTEGER NOT NULL,"
        "state INTEGER NOT NULL)"))
      return false;
  }
  return true;
}

bool DownloadDatabase::DropDownloadTable() {
  return GetDB().Execute("DROP TABLE downloads");
}

void DownloadDatabase::QueryDownloads(
    std::vector<DownloadPersistentStoreInfo>* results) {
  results->clear();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, full_path, url, start_time, received_bytes, "
        "total_bytes, state "
      "FROM downloads "
      "ORDER BY start_time"));
  if (!statement)
    return;

  while (statement.Step()) {
    DownloadPersistentStoreInfo info;
    info.db_handle = statement.ColumnInt64(0);

    info.path = ColumnFilePath(statement, 1);
    info.url = GURL(statement.ColumnString(2));
    info.start_time = base::Time::FromTimeT(statement.ColumnInt64(3));
    info.received_bytes = statement.ColumnInt64(4);
    info.total_bytes = statement.ColumnInt64(5);
    info.state = statement.ColumnInt(6);
    results->push_back(info);
  }
}

bool DownloadDatabase::UpdateDownload(int64 received_bytes,
                                      int32 state,
                                      DownloadID db_handle) {
  DCHECK(db_handle > 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads "
      "SET received_bytes=?, state=? WHERE id=?"));
  if (!statement)
    return false;

  statement.BindInt64(0, received_bytes);
  statement.BindInt(1, state);
  statement.BindInt64(2, db_handle);
  return statement.Run();
}

bool DownloadDatabase::UpdateDownloadPath(const FilePath& path,
                                          DownloadID db_handle) {
  DCHECK(db_handle > 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET full_path=? WHERE id=?"));
  if (!statement)
    return false;

  BindFilePath(statement, path, 0);
  statement.BindInt64(1, db_handle);
  return statement.Run();
}

bool DownloadDatabase::CleanUpInProgressEntries() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET state=? WHERE state=?"));
  if (!statement)
    return false;
  statement.BindInt(0, DownloadItem::CANCELLED);
  statement.BindInt(1, DownloadItem::IN_PROGRESS);
  return statement.Run();
}

int64 DownloadDatabase::CreateDownload(
    const DownloadPersistentStoreInfo& info) {
  if (owning_thread_set_) {
    CHECK_EQ(owning_thread_, base::PlatformThread::CurrentId());
  } else {
    owning_thread_ = base::PlatformThread::CurrentId();
    owning_thread_set_ = true;
  }

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO downloads "
      "(full_path, url, start_time, received_bytes, total_bytes, state) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  if (!statement)
    return 0;

  BindFilePath(statement, info.path, 0);
  statement.BindString(1, info.url.spec());
  statement.BindInt64(2, info.start_time.ToTimeT());
  statement.BindInt64(3, info.received_bytes);
  statement.BindInt64(4, info.total_bytes);
  statement.BindInt(5, info.state);

  if (statement.Run()) {
    int64 id = GetDB().GetLastInsertRowId();

    CHECK_EQ(0u, returned_ids_.count(id));
    returned_ids_.insert(id);

    return id;
  }
  return 0;
}

void DownloadDatabase::RemoveDownload(DownloadID db_handle) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  if (!statement)
    return;

  statement.BindInt64(0, db_handle);
  statement.Run();

  returned_ids_.erase(db_handle);
}

void DownloadDatabase::RemoveDownloadsBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  time_t start_time = delete_begin.ToTimeT();
  time_t end_time = delete_end.ToTimeT();

  // TODO(rdsmith): Remove when http://crbug.com/96627 is resolved.
  {
    sql::Statement dbg_statement(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT id FROM downloads WHERE start_time >= ? AND start_time < ? "
        "AND (State = ? OR State = ? OR State = ?)"));
    if (!dbg_statement)
      return;
    dbg_statement.BindInt64(0, start_time);
    dbg_statement.BindInt64(
        1,
        end_time ? end_time : std::numeric_limits<int64>::max());
    dbg_statement.BindInt(2, DownloadItem::COMPLETE);
    dbg_statement.BindInt(3, DownloadItem::CANCELLED);
    dbg_statement.BindInt(4, DownloadItem::INTERRUPTED);
    while (dbg_statement.Step()) {
      int64 id_to_delete = dbg_statement.ColumnInt64(0);
      returned_ids_.erase(id_to_delete);
    }
  }

  // This does not use an index. We currently aren't likely to have enough
  // downloads where an index by time will give us a lot of benefit.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE start_time >= ? AND start_time < ? "
      "AND (State = ? OR State = ? OR State = ?)"));
  if (!statement)
    return;

  statement.BindInt64(0, start_time);
  statement.BindInt64(
      1,
      end_time ? end_time : std::numeric_limits<int64>::max());
  statement.BindInt(2, DownloadItem::COMPLETE);
  statement.BindInt(3, DownloadItem::CANCELLED);
  statement.BindInt(4, DownloadItem::INTERRUPTED);
  statement.Run();
}

}  // namespace history
