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
#include "chrome/browser/history/download_row.h"
#include "content/public/browser/download_item.h"
#include "sql/statement.h"

using content::DownloadItem;

namespace history {

// static
const int64 DownloadDatabase::kUninitializedHandle = -1;

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

// These constants and next two functions are used to allow
// DownloadItem::DownloadState to change without breaking the database schema.
// They guarantee that the values of the |state| field in the database are one
// of the values returned by StateToInt, and that the values of the |state|
// field of the DownloadRows returned by QueryDownloads() are one of the values
// returned by IntToState().
static const int kStateInvalid = -1;
static const int kStateInProgress = 0;
static const int kStateComplete = 1;
static const int kStateCancelled = 2;
static const int kStateBug140687 = 3;
static const int kStateInterrupted = 4;

int StateToInt(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS: return kStateInProgress;
    case DownloadItem::COMPLETE: return kStateComplete;
    case DownloadItem::CANCELLED: return kStateCancelled;
    case DownloadItem::INTERRUPTED: return kStateInterrupted;
    case DownloadItem::MAX_DOWNLOAD_STATE: return kStateInvalid;
    default: return kStateInvalid;
  }
}

DownloadItem::DownloadState IntToState(int state) {
  switch (state) {
    case kStateInProgress: return DownloadItem::IN_PROGRESS;
    case kStateComplete: return DownloadItem::COMPLETE;
    case kStateCancelled: return DownloadItem::CANCELLED;
    // We should not need kStateBug140687 here because MigrateDownloadState()
    // is called in HistoryDatabase::Init().
    case kStateInterrupted: return DownloadItem::INTERRUPTED;
    default: return DownloadItem::MAX_DOWNLOAD_STATE;
  }
}

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

bool DownloadDatabase::EnsureColumnExists(
    const std::string& name, const std::string& type) {
  std::string add_col = "ALTER TABLE downloads ADD COLUMN " + name + " " + type;
  return GetDB().DoesColumnExist("downloads", name.c_str()) ||
         GetDB().Execute(add_col.c_str());
}

bool DownloadDatabase::MigrateDownloadsState() {
  sql::Statement statement(GetDB().GetUniqueStatement(
        "UPDATE downloads SET state=? WHERE state=?"));
  statement.BindInt(0, kStateInterrupted);
  statement.BindInt(1, kStateBug140687);
  return statement.Run();
}

bool DownloadDatabase::InitDownloadTable() {
  GetMetaTable().GetValue(kNextDownloadId, &next_id_);
  if (GetDB().DoesTableExist("downloads")) {
    return EnsureColumnExists("end_time", "INTEGER NOT NULL DEFAULT 0") &&
           EnsureColumnExists("opened", "INTEGER NOT NULL DEFAULT 0");
  } else {
    return GetDB().Execute(kSchema);
  }
}

bool DownloadDatabase::DropDownloadTable() {
  return GetDB().Execute("DROP TABLE downloads");
}

void DownloadDatabase::QueryDownloads(
    std::vector<DownloadRow>* results) {
  DCHECK(results);
  results->clear();
  if (next_db_handle_ < 1)
    next_db_handle_ = 1;
  std::set<int64> db_handles;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, full_path, url, start_time, received_bytes, "
        "total_bytes, state, end_time, opened "
      "FROM downloads "
      "ORDER BY start_time"));

  while (statement.Step()) {
    DownloadRow info;
    info.db_handle = statement.ColumnInt64(0);
    info.path = ColumnFilePath(statement, 1);
    info.url = GURL(statement.ColumnString(2));
    info.start_time = base::Time::FromTimeT(statement.ColumnInt64(3));
    info.received_bytes = statement.ColumnInt64(4);
    info.total_bytes = statement.ColumnInt64(5);
    int state = statement.ColumnInt(6);
    info.state = IntToState(state);
    info.end_time = base::Time::FromTimeT(statement.ColumnInt64(7));
    info.opened = statement.ColumnInt(8) != 0;
    if (info.db_handle >= next_db_handle_)
      next_db_handle_ = info.db_handle + 1;
    if (!db_handles.insert(info.db_handle).second) {
      // info.db_handle was already in db_handles. The database is corrupt.
      base::debug::Alias(&info.db_handle);
      DCHECK(false);
    }
    if (info.state == DownloadItem::MAX_DOWNLOAD_STATE) {
      UMA_HISTOGRAM_COUNTS("Download.DatabaseInvalidState", state);
      continue;
    }
    results->push_back(info);
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadRow& data) {
  DCHECK(data.db_handle > 0);
  int state = StateToInt(data.state);
  if (state == kStateInvalid) {
    // TODO(benjhayden) [D]CHECK instead.
    return false;
  }
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads "
      "SET full_path=?, received_bytes=?, state=?, end_time=?, total_bytes=?, "
      "opened=? WHERE id=?"));
  BindFilePath(statement, data.path, 0);
  statement.BindInt64(1, data.received_bytes);
  statement.BindInt(2, state);
  statement.BindInt64(3, data.end_time.ToTimeT());
  statement.BindInt(4, data.total_bytes);
  statement.BindInt(5, (data.opened ? 1 : 0));
  statement.BindInt64(6, data.db_handle);

  return statement.Run();
}

bool DownloadDatabase::CleanUpInProgressEntries() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET state=? WHERE state=?"));
  statement.BindInt(0, kStateCancelled);
  statement.BindInt(1, kStateInProgress);

  return statement.Run();
}

int64 DownloadDatabase::CreateDownload(
    const DownloadRow& info) {
  if (next_db_handle_ == 0) {
    // This is unlikely. All current known tests and users already call
    // QueryDownloads() before CreateDownload().
    std::vector<DownloadRow> results;
    QueryDownloads(&results);
    CHECK_NE(0, next_db_handle_);
  }

  int state = StateToInt(info.state);
  if (state == kStateInvalid)
    return kUninitializedHandle;

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
  statement.BindInt(6, state);
  statement.BindInt64(7, info.end_time.ToTimeT());
  statement.BindInt(8, info.opened ? 1 : 0);

  if (statement.Run()) {
    // TODO(benjhayden) if(info.id>next_id_){setvalue;next_id_=info.id;}
    GetMetaTable().SetValue(kNextDownloadId, ++next_id_);

    return db_handle;
  }
  return kUninitializedHandle;
}

void DownloadDatabase::RemoveDownload(int64 handle) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  statement.BindInt64(0, handle);
  statement.Run();
}

int DownloadDatabase::CountDownloads() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT count(*) from downloads"));
  statement.Step();
  return statement.ColumnInt(0);
}

}  // namespace history
