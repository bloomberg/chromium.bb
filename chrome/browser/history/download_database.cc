// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_database.h"

#include <limits>
#include <string>
#include <vector>

#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_types.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "sql/statement.h"

using content::DownloadItem;

namespace history {

// static
const int64 DownloadDatabase::kUninitializedHandle = -1;

namespace {

// Reason for dropping a particular record.
enum DroppedReason {
  DROPPED_REASON_BAD_STATE = 0,
  DROPPED_REASON_BAD_DANGER_TYPE = 1,
  DROPPED_REASON_MAX
};

static const char kSchema[] =
  "CREATE TABLE downloads ("
  "id INTEGER PRIMARY KEY,"           // Primary key.
  "current_path LONGVARCHAR NOT NULL,"  // Current disk location of the download
  "target_path LONGVARCHAR NOT NULL,"  // Final disk location of the download
  "start_time INTEGER NOT NULL,"      // When the download was started.
  "received_bytes INTEGER NOT NULL,"  // Total size downloaded.
  "total_bytes INTEGER NOT NULL,"     // Total size of the download.
  "state INTEGER NOT NULL,"           // 1=complete, 4=interrupted
  "danger_type INTEGER NOT NULL, "    // Not dangerous, danger type, validated.
  "interrupt_reason INTEGER NOT NULL,"  // Reason the download was interrupted.
  "end_time INTEGER NOT NULL,"        // When the download completed.
  "opened INTEGER NOT NULL)";         // 1 if it has ever been opened else 0

static const char kUrlChainSchema[] =
    "CREATE TABLE downloads_url_chains ("
    "id INTEGER NOT NULL,"           // downloads.id.
    "chain_index INTEGER NOT NULL,"  // Index of url in chain
                                     // 0 is initial target,
                                     // MAX is target after redirects.
    "url LONGVARCHAR NOT NULL, "     // URL.
    "PRIMARY KEY (id, chain_index) )";

// These constants and next two functions are used to allow
// DownloadItem::DownloadState and DownloadDangerType to change without
// breaking the database schema.
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

static const int kDangerTypeInvalid = -1;
static const int kDangerTypeNotDangerous = 0;
static const int kDangerTypeDangerousFile = 1;
static const int kDangerTypeDangerousUrl = 2;
static const int kDangerTypeDangerousContent = 3;
static const int kDangerTypeMaybeDangerousContent = 4;
static const int kDangerTypeUncommonContent = 5;
static const int kDangerTypeUserValidated = 6;
static const int kDangerTypeDangerousHost = 7;

int StateToInt(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS: return kStateInProgress;
    case DownloadItem::COMPLETE: return kStateComplete;
    case DownloadItem::CANCELLED: return kStateCancelled;
    case DownloadItem::INTERRUPTED: return kStateInterrupted;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      return kStateInvalid;
  }
  NOTREACHED();
  return kStateInvalid;
}

DownloadItem::DownloadState IntToState(int state) {
  switch (state) {
    case kStateInProgress: return DownloadItem::IN_PROGRESS;
    case kStateComplete: return DownloadItem::COMPLETE;
    case kStateCancelled: return DownloadItem::CANCELLED;
    // We should not need kStateBug140687 here because MigrateDownloadsState()
    // is called in HistoryDatabase::Init().
    case kStateInterrupted: return DownloadItem::INTERRUPTED;
    default: return DownloadItem::MAX_DOWNLOAD_STATE;
  }
}

int DangerTypeToInt(content::DownloadDangerType danger_type) {
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return kDangerTypeNotDangerous;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return kDangerTypeDangerousFile;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return kDangerTypeDangerousUrl;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return kDangerTypeDangerousContent;
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return kDangerTypeMaybeDangerousContent;
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return kDangerTypeUncommonContent;
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return kDangerTypeUserValidated;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return kDangerTypeDangerousHost;
    case content::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      return kDangerTypeInvalid;
  }
  NOTREACHED();
  return kDangerTypeInvalid;
}

content::DownloadDangerType IntToDangerType(int danger_type) {
  switch (danger_type) {
    case kDangerTypeNotDangerous:
      return content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case kDangerTypeDangerousFile:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    case kDangerTypeDangerousUrl:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
    case kDangerTypeDangerousContent:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
    case kDangerTypeMaybeDangerousContent:
      return content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    case kDangerTypeUncommonContent:
      return content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
    case kDangerTypeUserValidated:
      return content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED;
    case kDangerTypeDangerousHost:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
    default:
      return content::DOWNLOAD_DANGER_TYPE_MAX;
  }
}

#if defined(OS_POSIX)

// Binds/reads the given file path to the given column of the given statement.
void BindFilePath(sql::Statement& statement, const base::FilePath& path,
                  int col) {
  statement.BindString(col, path.value());
}
base::FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return base::FilePath(statement.ColumnString(col));
}

#else

// See above.
void BindFilePath(sql::Statement& statement, const base::FilePath& path,
                  int col) {
  statement.BindString16(col, path.value());
}
base::FilePath ColumnFilePath(sql::Statement& statement, int col) {
  return base::FilePath(statement.ColumnString16(col));
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

bool DownloadDatabase::MigrateDownloadsReasonPathsAndDangerType() {
  // We need to rename the table and copy back from it because SQLite
  // provides no way to rename or delete a column.
  if (!GetDB().Execute("ALTER TABLE downloads RENAME TO downloads_tmp"))
    return false;

  // Recreate main table.
  if (!GetDB().Execute(kSchema))
    return false;

  // Populate it.  As we do so, we transform the time values from time_t
  // (seconds since 1/1/1970 UTC), to our internal measure (microseconds
  // since the Windows Epoch).  Note that this is dependent on the
  // internal representation of base::Time and needs to change if that changes.
  sql::Statement statement_populate(GetDB().GetUniqueStatement(
    "INSERT INTO downloads "
    "( id, current_path, target_path, start_time, received_bytes, total_bytes, "
    "  state, danger_type, interrupt_reason, end_time, opened ) "
    "SELECT id, full_path, full_path, "
    "       CASE start_time WHEN 0 THEN 0 ELSE "
    "            (start_time + 11644473600) * 1000000 END, "
    "       received_bytes, total_bytes, "
    "       state, ?, ?, "
    "       CASE end_time WHEN 0 THEN 0 ELSE "
    "            (end_time + 11644473600) * 1000000 END, "
    "       opened "
    "FROM downloads_tmp"));
  statement_populate.BindInt(0, content::DOWNLOAD_INTERRUPT_REASON_NONE);
  statement_populate.BindInt(1, kDangerTypeNotDangerous);
  if (!statement_populate.Run())
    return false;

  // Create new chain table and populate it.
  if (!GetDB().Execute(kUrlChainSchema))
    return false;

  if (!GetDB().Execute("INSERT INTO downloads_url_chains "
                       "  ( id, chain_index, url) "
                       "  SELECT id, 0, url from downloads_tmp"))
    return false;

  // Get rid of temporary table.
  if (!GetDB().Execute("DROP TABLE downloads_tmp"))
    return false;

  return true;
}

bool DownloadDatabase::InitDownloadTable() {
  GetMetaTable().GetValue(kNextDownloadId, &next_id_);
  if (GetDB().DoesTableExist("downloads")) {
    return EnsureColumnExists("end_time", "INTEGER NOT NULL DEFAULT 0") &&
           EnsureColumnExists("opened", "INTEGER NOT NULL DEFAULT 0");
  } else {
    // If the "downloads" table doesn't exist, the downloads_url_chain
    // table better not.
    return (!GetDB().DoesTableExist("downloads_url_chain") &&
            GetDB().Execute(kSchema) && GetDB().Execute(kUrlChainSchema));
  }
}

bool DownloadDatabase::DropDownloadTable() {
  return GetDB().Execute("DROP TABLE downloads");
}

void DownloadDatabase::QueryDownloads(
    std::vector<DownloadRow>* results) {
  results->clear();
  if (next_db_handle_ < 1)
    next_db_handle_ = 1;
  std::set<int64> db_handles;

  std::map<DownloadID, DownloadRow*> info_map;

  sql::Statement statement_main(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, current_path, target_path, start_time, received_bytes, "
        "total_bytes, state, danger_type, interrupt_reason, end_time, opened "
      "FROM downloads "
      "ORDER BY start_time"));

  while (statement_main.Step()) {
    scoped_ptr<DownloadRow> info(new DownloadRow());
    int column = 0;

    int db_handle = statement_main.ColumnInt64(column++);
    info->db_handle = db_handle;
    info->current_path = ColumnFilePath(statement_main, column++);
    info->target_path = ColumnFilePath(statement_main, column++);
    info->start_time = base::Time::FromInternalValue(
        statement_main.ColumnInt64(column++));
    info->received_bytes = statement_main.ColumnInt64(column++);
    info->total_bytes = statement_main.ColumnInt64(column++);
    int state = statement_main.ColumnInt(column++);
    info->state = IntToState(state);
    if (info->state == DownloadItem::MAX_DOWNLOAD_STATE)
      UMA_HISTOGRAM_COUNTS("Download.DatabaseInvalidState", state);
    info->danger_type = IntToDangerType(statement_main.ColumnInt(column++));
    info->interrupt_reason = static_cast<content::DownloadInterruptReason>(
        statement_main.ColumnInt(column++));
    info->end_time = base::Time::FromInternalValue(
        statement_main.ColumnInt64(column++));
    info->opened = statement_main.ColumnInt(column++) != 0;
    if (info->db_handle >= next_db_handle_)
      next_db_handle_ = info->db_handle + 1;
    if (!db_handles.insert(info->db_handle).second) {
      // info->db_handle was already in db_handles. The database is corrupt.
      base::debug::Alias(&info->db_handle);
      DCHECK(false);
    }

    // If the record is corrupted, note that and drop it.
    if (info->state == DownloadItem::MAX_DOWNLOAD_STATE ||
        info->danger_type == content::DOWNLOAD_DANGER_TYPE_MAX) {
      DroppedReason reason = DROPPED_REASON_MAX;
      if (info->state == DownloadItem::MAX_DOWNLOAD_STATE)
        reason = DROPPED_REASON_BAD_STATE;
      else if (info->danger_type == content::DOWNLOAD_DANGER_TYPE_MAX)
        reason = DROPPED_REASON_BAD_DANGER_TYPE;
      else
        NOTREACHED();

      UMA_HISTOGRAM_ENUMERATION(
          "Download.DatabaseRecordDropped", reason, DROPPED_REASON_MAX + 1);

      continue;
    }

    DCHECK(!ContainsKey(info_map, info->db_handle));
    info_map[db_handle] = info.release();
  }

  sql::Statement statement_chain(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, chain_index, url FROM downloads_url_chains "
      "ORDER BY id, chain_index"));

  while (statement_chain.Step()) {
    int column = 0;
    int64 db_handle = statement_chain.ColumnInt64(column++);
    int chain_index = statement_chain.ColumnInt(column++);

    // Note that these DCHECKs may trip as a result of corrupted databases.
    // We have them because in debug builds the chances are higher there's
    // an actual bug than that the database is corrupt, but we handle the
    // DB corruption case in production code.

    // Confirm the handle has already been seen--if it hasn't, discard the
    // record.
    DCHECK(ContainsKey(info_map, db_handle));
    if (!ContainsKey(info_map, db_handle))
      continue;

    // Confirm all previous URLs in the chain have already been seen;
    // if not, fill in with null or discard record.
    int current_chain_size = info_map[db_handle]->url_chain.size();
    std::vector<GURL>* url_chain(&info_map[db_handle]->url_chain);
    DCHECK_EQ(chain_index, current_chain_size);
    while (current_chain_size < chain_index) {
      url_chain->push_back(GURL());
      current_chain_size++;
    }
    if (current_chain_size > chain_index)
      continue;

    // Save the record.
    url_chain->push_back(GURL(statement_chain.ColumnString(2)));
  }

  for (std::map<DownloadID, DownloadRow*>::iterator
           it = info_map.begin(); it != info_map.end(); ++it) {
    DownloadRow* row = it->second;
    bool empty_url_chain = row->url_chain.empty();
    UMA_HISTOGRAM_BOOLEAN("Download.DatabaseEmptyUrlChain", empty_url_chain);
    if (empty_url_chain) {
      RemoveDownload(row->db_handle);
    } else {
      // Copy the contents of the stored info.
      results->push_back(*row);
    }
    delete row;
    it->second = NULL;
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadRow& data) {
  DCHECK(data.db_handle > 0);
  int state = StateToInt(data.state);
  if (state == kStateInvalid) {
    NOTREACHED();
    return false;
  }
  int danger_type = DangerTypeToInt(data.danger_type);
  if (danger_type == kDangerTypeInvalid) {
    NOTREACHED();
    return false;
  }

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads "
      "SET current_path=?, target_path=?, received_bytes=?, state=?, "
          "danger_type=?, interrupt_reason=?, end_time=?, total_bytes=?, "
          "opened=? WHERE id=?"));
  int column = 0;
  BindFilePath(statement, data.current_path, column++);
  BindFilePath(statement, data.target_path, column++);
  statement.BindInt64(column++, data.received_bytes);
  statement.BindInt(column++, state);
  statement.BindInt(column++, danger_type);
  statement.BindInt(column++, static_cast<int>(data.interrupt_reason));
  statement.BindInt64(column++, data.end_time.ToInternalValue());
  statement.BindInt(column++, data.total_bytes);
  statement.BindInt(column++, (data.opened ? 1 : 0));
  statement.BindInt64(column++, data.db_handle);

  return statement.Run();
}

bool DownloadDatabase::CleanUpInProgressEntries() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET state=? WHERE state=?"));
  statement.BindInt(0, kStateCancelled);
  statement.BindInt(1, kStateInProgress);

  return statement.Run();
}

int64 DownloadDatabase::CreateDownload(const DownloadRow& info) {
  if (next_db_handle_ == 0) {
    // This is unlikely. All current known tests and users already call
    // QueryDownloads() before CreateDownload().
    std::vector<DownloadRow> results;
    QueryDownloads(&results);
    CHECK_NE(0, next_db_handle_);
  }

  if (info.url_chain.empty())
    return kUninitializedHandle;

  int state = StateToInt(info.state);
  if (state == kStateInvalid)
    return kUninitializedHandle;

  int danger_type = DangerTypeToInt(info.danger_type);
  if (danger_type == kDangerTypeInvalid)
    return kUninitializedHandle;

  int db_handle = next_db_handle_++;

  {
    sql::Statement statement_insert(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO downloads "
        "(id, current_path, target_path, start_time, "
        " received_bytes, total_bytes, state, danger_type, interrupt_reason, "
        " end_time, opened) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int column = 0;
    statement_insert.BindInt64(column++, db_handle);
    BindFilePath(statement_insert, info.current_path, column++);
    BindFilePath(statement_insert, info.target_path, column++);
    statement_insert.BindInt64(column++, info.start_time.ToInternalValue());
    statement_insert.BindInt64(column++, info.received_bytes);
    statement_insert.BindInt64(column++, info.total_bytes);
    statement_insert.BindInt(column++, state);
    statement_insert.BindInt(column++, danger_type);
    statement_insert.BindInt(column++, info.interrupt_reason);
    statement_insert.BindInt64(column++, info.end_time.ToInternalValue());
    statement_insert.BindInt(column++, info.opened ? 1 : 0);
    if (!statement_insert.Run()) {
      // GetErrorCode() returns a bitmask where the lower byte is a more general
      // code and the upper byte is a more specific code. In order to save
      // memory, take the general code, of which there are fewer than 50. See
      // also sql/connection.cc
      // http://www.sqlite.org/c3ref/c_abort_rollback.html
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      return kUninitializedHandle;
    }
  }

  {
    sql::Statement count_urls(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "SELECT count(*) FROM downloads_url_chains WHERE id=?"));
    count_urls.BindInt64(0, db_handle);
    if (count_urls.Step()) {
      bool corrupt_urls = count_urls.ColumnInt(0) > 0;
      UMA_HISTOGRAM_BOOLEAN("Download.DatabaseCorruptUrls", corrupt_urls);
      if (corrupt_urls) {
        // There should not be any URLs in downloads_url_chains for this
        // db_handle.  If there are, we don't want them to interfere with
        // inserting the correct URLs, so just remove them.
        RemoveDownloadURLs(db_handle);
      }
    }
  }

  sql::Statement statement_insert_chain(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO downloads_url_chains "
                                 "(id, chain_index, url) "
                                 "VALUES (?, ?, ?)"));
  for (size_t i = 0; i < info.url_chain.size(); ++i) {
    statement_insert_chain.BindInt64(0, db_handle);
    statement_insert_chain.BindInt(1, i);
    statement_insert_chain.BindString(2, info.url_chain[i].spec());
    if (!statement_insert_chain.Run()) {
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      RemoveDownload(db_handle);
      return kUninitializedHandle;
    }
    statement_insert_chain.Reset(true);
  }

  // TODO(benjhayden) if(info.id>next_id_){setvalue;next_id_=info.id;}
  GetMetaTable().SetValue(kNextDownloadId, ++next_id_);
  return db_handle;
}

void DownloadDatabase::RemoveDownload(int64 handle) {
  sql::Statement downloads_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  downloads_statement.BindInt64(0, handle);
  if (!downloads_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
    return;
  }
  RemoveDownloadURLs(handle);
}

void DownloadDatabase::RemoveDownloadURLs(int64 handle) {
  sql::Statement urlchain_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads_url_chains WHERE id=?"));
  urlchain_statement.BindInt64(0, handle);
  if (!urlchain_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
  }
}

int DownloadDatabase::CountDownloads() {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT count(*) from downloads"));
  statement.Step();
  return statement.ColumnInt(0);
}

}  // namespace history
