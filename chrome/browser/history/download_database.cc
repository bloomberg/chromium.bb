// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_database.h"

#include <limits>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "sql/statement.h"

// TODO(benjhayden): Change this to DCHECK when we have more debugging
// information from the next dev cycle, before the next stable/beta branch is
// cut, in order to prevent unnecessary crashes on those channels. If we still
// don't have root cause before the dev cycle after the next stable/beta
// releases, uncomment it out to re-enable debugging checks. Whenever this macro
// is toggled, the corresponding macro in download_manager_impl.cc should also
// be toggled. When 96627 is fixed, this macro and all its usages and
// returned_ids_ can be deleted or permanently changed to DCHECK as appropriate.
#define CHECK_96627 CHECK

using content::DownloadItem;

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
  statement.BindString(col, UTF16ToUTF8(path.value()));
}
FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return FilePath(UTF8ToUTF16(statement.ColumnString(col)));
}

#endif

// Key in the meta_table containing the next id to use for a new download in
// this profile.
static const char kNextDownloadId[] = "next_download_id";

}  // namespace

DownloadDatabase::DownloadDatabase()
    : owning_thread_set_(false),
      owning_thread_(0),
      next_id_(0) {
}

DownloadDatabase::~DownloadDatabase() {
}

void DownloadDatabase::CheckThread() {
  if (owning_thread_set_) {
    CHECK_96627(owning_thread_ == base::PlatformThread::CurrentId());
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
  bool success = meta_table_.Init(&GetDB(), 0, 0);
  DCHECK(success);
  meta_table_.GetValue(kNextDownloadId, &next_id_);
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

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, full_path, url, start_time, received_bytes, "
        "total_bytes, state, end_time, opened "
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
    info.end_time = base::Time::FromTimeT(statement.ColumnInt64(7));
    info.opened = statement.ColumnInt(8) != 0;
    results->push_back(info);

    // TODO(rdsmith): Remove when http://crbug.com/96627 is resolved.
    // This path is used for the initial loading of the downloads
    // from the history.  Insert ids into returned_ids_ to initialize it.
    // We don't assert on the id not already being there in case
    // this call is used on some path other than the initialization path.
    if (returned_ids_.count(info.db_handle) == 0)
      returned_ids_.insert(info.db_handle);
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadPersistentStoreInfo& data) {
  CheckThread();
  DCHECK(data.db_handle > 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads "
      "SET received_bytes=?, state=?, end_time=?, opened=? WHERE id=?"));
  if (!statement)
    return false;

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
  if (!statement)
    return false;

  BindFilePath(statement, path, 0);
  statement.BindInt64(1, db_handle);
  return statement.Run();
}

bool DownloadDatabase::CleanUpInProgressEntries() {
  CheckThread();
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
  CheckThread();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO downloads "
      "(full_path, url, start_time, received_bytes, total_bytes, state, "
      "end_time, opened) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
  if (!statement)
    return 0;

  BindFilePath(statement, info.path, 0);
  statement.BindString(1, info.url.spec());
  statement.BindInt64(2, info.start_time.ToTimeT());
  statement.BindInt64(3, info.received_bytes);
  statement.BindInt64(4, info.total_bytes);
  statement.BindInt(5, info.state);
  statement.BindInt64(6, info.end_time.ToTimeT());
  statement.BindInt(7, info.opened ? 1 : 0);

  if (statement.Run()) {
    int64 id = GetDB().GetLastInsertRowId();

    if (returned_ids_.count(id) != 0) {
      // We have an invariant violation and we're going to crash.  Take a
      // moment more before crashing to figure out if it's a returned_ids_/DB
      // inconsistency, or an inconsistency inside the DB.
      sql::Statement dbg_statement(GetDB().GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT id FROM downloads;"));
      CHECK_96627(dbg_statement);

      std::set<int64> database_ids;
      while (dbg_statement.Step()) {
        bool success = database_ids.insert(dbg_statement.ColumnInt64(0)).second;
        CHECK_96627(success);
      }
      CHECK_96627(false);
    }

    returned_ids_.insert(id);

    // TODO(benjhayden) if(info.id>next_id_){setvalue;next_id_=info.id;}
    meta_table_.SetValue(kNextDownloadId, ++next_id_);

    return id;
  }
  return 0;
}

void DownloadDatabase::RemoveDownload(DownloadID db_handle) {
  CheckThread();
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  if (!statement)
    return;

  statement.BindInt64(0, db_handle);
  if (statement.Run())
    // TODO(rdsmith): Remove when http://crbug.com/96627 is resolved.
    returned_ids_.erase(db_handle);
}

void DownloadDatabase::RemoveDownloadsBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  CheckThread();
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
    CHECK_96627(dbg_statement.Succeeded());
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
