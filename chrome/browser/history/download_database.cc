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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/history/download_row.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "sql/statement.h"

using content::DownloadItem;

namespace history {

namespace {

// Reason for dropping a particular record.
enum DroppedReason {
  DROPPED_REASON_BAD_STATE = 0,
  DROPPED_REASON_BAD_DANGER_TYPE = 1,
  DROPPED_REASON_BAD_ID = 2,
  DROPPED_REASON_DUPLICATE_ID = 3,
  DROPPED_REASON_MAX
};

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

}  // namespace

// These constants and the transformation functions below are used to allow
// DownloadItem::DownloadState and DownloadDangerType to change without
// breaking the database schema.
// They guarantee that the values of the |state| field in the database are one
// of the values returned by StateToInt, and that the values of the |state|
// field of the DownloadRows returned by QueryDownloads() are one of the values
// returned by IntToState().
const int DownloadDatabase::kStateInvalid = -1;
const int DownloadDatabase::kStateInProgress = 0;
const int DownloadDatabase::kStateComplete = 1;
const int DownloadDatabase::kStateCancelled = 2;
const int DownloadDatabase::kStateBug140687 = 3;
const int DownloadDatabase::kStateInterrupted = 4;

const int DownloadDatabase::kDangerTypeInvalid = -1;
const int DownloadDatabase::kDangerTypeNotDangerous = 0;
const int DownloadDatabase::kDangerTypeDangerousFile = 1;
const int DownloadDatabase::kDangerTypeDangerousUrl = 2;
const int DownloadDatabase::kDangerTypeDangerousContent = 3;
const int DownloadDatabase::kDangerTypeMaybeDangerousContent = 4;
const int DownloadDatabase::kDangerTypeUncommonContent = 5;
const int DownloadDatabase::kDangerTypeUserValidated = 6;
const int DownloadDatabase::kDangerTypeDangerousHost = 7;
const int DownloadDatabase::kDangerTypePotentiallyUnwanted = 8;

int DownloadDatabase::StateToInt(DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS: return DownloadDatabase::kStateInProgress;
    case DownloadItem::COMPLETE: return DownloadDatabase::kStateComplete;
    case DownloadItem::CANCELLED: return DownloadDatabase::kStateCancelled;
    case DownloadItem::INTERRUPTED: return DownloadDatabase::kStateInterrupted;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      return DownloadDatabase::kStateInvalid;
  }
  NOTREACHED();
  return DownloadDatabase::kStateInvalid;
}

DownloadItem::DownloadState DownloadDatabase::IntToState(int state) {
  switch (state) {
    case DownloadDatabase::kStateInProgress: return DownloadItem::IN_PROGRESS;
    case DownloadDatabase::kStateComplete: return DownloadItem::COMPLETE;
    case DownloadDatabase::kStateCancelled: return DownloadItem::CANCELLED;
    // We should not need kStateBug140687 here because MigrateDownloadsState()
    // is called in HistoryDatabase::Init().
    case DownloadDatabase::kStateInterrupted: return DownloadItem::INTERRUPTED;
    default: return DownloadItem::MAX_DOWNLOAD_STATE;
  }
}

int DownloadDatabase::DangerTypeToInt(content::DownloadDangerType danger_type) {
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return DownloadDatabase::kDangerTypeNotDangerous;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return DownloadDatabase::kDangerTypeDangerousFile;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return DownloadDatabase::kDangerTypeDangerousUrl;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return DownloadDatabase::kDangerTypeDangerousContent;
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return DownloadDatabase::kDangerTypeMaybeDangerousContent;
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return DownloadDatabase::kDangerTypeUncommonContent;
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return DownloadDatabase::kDangerTypeUserValidated;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return DownloadDatabase::kDangerTypeDangerousHost;
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return DownloadDatabase::kDangerTypePotentiallyUnwanted;
    case content::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      return DownloadDatabase::kDangerTypeInvalid;
  }
  NOTREACHED();
  return DownloadDatabase::kDangerTypeInvalid;
}

content::DownloadDangerType DownloadDatabase::IntToDangerType(int danger_type) {
  switch (danger_type) {
    case DownloadDatabase::kDangerTypeNotDangerous:
      return content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case DownloadDatabase::kDangerTypeDangerousFile:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    case DownloadDatabase::kDangerTypeDangerousUrl:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
    case DownloadDatabase::kDangerTypeDangerousContent:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
    case DownloadDatabase::kDangerTypeMaybeDangerousContent:
      return content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    case DownloadDatabase::kDangerTypeUncommonContent:
      return content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
    case DownloadDatabase::kDangerTypeUserValidated:
      return content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED;
    case DownloadDatabase::kDangerTypeDangerousHost:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
    case DownloadDatabase::kDangerTypePotentiallyUnwanted:
      return content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
    default:
      return content::DOWNLOAD_DANGER_TYPE_MAX;
  }
}

DownloadDatabase::DownloadDatabase()
    : owning_thread_set_(false),
      owning_thread_(0),
      in_progress_entry_cleanup_completed_(false) {
}

DownloadDatabase::~DownloadDatabase() {
}

bool DownloadDatabase::EnsureColumnExists(
    const std::string& name, const std::string& type) {
  std::string add_col = "ALTER TABLE downloads ADD COLUMN " + name + " " + type;
  return GetDB().DoesColumnExist("downloads", name.c_str()) ||
         GetDB().Execute(add_col.c_str());
}

bool DownloadDatabase::MigrateMimeType() {
  return EnsureColumnExists("mime_type", "VARCHAR(255) NOT NULL"
                            " DEFAULT \"\"") &&
         EnsureColumnExists("original_mime_type", "VARCHAR(255) NOT NULL"
                            " DEFAULT \"\"");
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

  const char kReasonPathDangerSchema[] =
      "CREATE TABLE downloads ("
      "id INTEGER PRIMARY KEY,"
      "current_path LONGVARCHAR NOT NULL,"
      "target_path LONGVARCHAR NOT NULL,"
      "start_time INTEGER NOT NULL,"
      "received_bytes INTEGER NOT NULL,"
      "total_bytes INTEGER NOT NULL,"
      "state INTEGER NOT NULL,"
      "danger_type INTEGER NOT NULL,"
      "interrupt_reason INTEGER NOT NULL,"
      "end_time INTEGER NOT NULL,"
      "opened INTEGER NOT NULL)";

  static const char kReasonPathDangerUrlChainSchema[] =
      "CREATE TABLE downloads_url_chains ("
      "id INTEGER NOT NULL,"                // downloads.id.
      "chain_index INTEGER NOT NULL,"       // Index of url in chain
                                            // 0 is initial target,
                                            // MAX is target after redirects.
      "url LONGVARCHAR NOT NULL, "          // URL.
      "PRIMARY KEY (id, chain_index) )";


  // Recreate main table.
  if (!GetDB().Execute(kReasonPathDangerSchema))
    return false;

  // Populate it.  As we do so, we transform the time values from time_t
  // (seconds since 1/1/1970 UTC), to our internal measure (microseconds
  // since the Windows Epoch).  Note that this is dependent on the
  // internal representation of base::Time and needs to change if that changes.
  sql::Statement statement_populate(GetDB().GetUniqueStatement(
      "INSERT INTO downloads "
      "( id, current_path, target_path, start_time, received_bytes, "
      "  total_bytes, state, danger_type, interrupt_reason, end_time, opened ) "
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
  if (!GetDB().Execute(kReasonPathDangerUrlChainSchema))
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

bool DownloadDatabase::MigrateReferrer() {
  return EnsureColumnExists("referrer", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::MigrateDownloadedByExtension() {
  return EnsureColumnExists("by_ext_id", "VARCHAR NOT NULL DEFAULT \"\"") &&
         EnsureColumnExists("by_ext_name", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::MigrateDownloadValidators() {
  return EnsureColumnExists("etag", "VARCHAR NOT NULL DEFAULT \"\"") &&
         EnsureColumnExists("last_modified", "VARCHAR NOT NULL DEFAULT \"\"");
}

bool DownloadDatabase::InitDownloadTable() {
  const char kSchema[] =
      "CREATE TABLE downloads ("
      "id INTEGER PRIMARY KEY,"             // Primary key.
      "current_path LONGVARCHAR NOT NULL,"  // Current disk location
      "target_path LONGVARCHAR NOT NULL,"   // Final disk location
      "start_time INTEGER NOT NULL,"        // When the download was started.
      "received_bytes INTEGER NOT NULL,"    // Total size downloaded.
      "total_bytes INTEGER NOT NULL,"       // Total size of the download.
      "state INTEGER NOT NULL,"             // 1=complete, 4=interrupted
      "danger_type INTEGER NOT NULL,"       // Danger type, validated.
      "interrupt_reason INTEGER NOT NULL,"  // content::DownloadInterruptReason
      "end_time INTEGER NOT NULL,"          // When the download completed.
      "opened INTEGER NOT NULL,"            // 1 if it has ever been opened
                                            // else 0
      "referrer VARCHAR NOT NULL,"          // HTTP Referrer
      "by_ext_id VARCHAR NOT NULL,"         // ID of extension that started the
                                            // download
      "by_ext_name VARCHAR NOT NULL,"       // name of extension
      "etag VARCHAR NOT NULL,"              // ETag
      "last_modified VARCHAR NOT NULL,"     // Last-Modified header
      "mime_type VARCHAR(255) NOT NULL,"    // MIME type.
      "original_mime_type VARCHAR(255) NOT NULL)";  // Original MIME type.

  const char kUrlChainSchema[] =
      "CREATE TABLE downloads_url_chains ("
      "id INTEGER NOT NULL,"                // downloads.id.
      "chain_index INTEGER NOT NULL,"       // Index of url in chain
                                            // 0 is initial target,
                                            // MAX is target after redirects.
      "url LONGVARCHAR NOT NULL, "          // URL.
      "PRIMARY KEY (id, chain_index) )";

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

uint32 DownloadDatabase::GetNextDownloadId() {
  sql::Statement select_max_id(GetDB().GetUniqueStatement(
      "SELECT max(id) FROM downloads"));
  bool result = select_max_id.Step();
  DCHECK(result);
  // If there are zero records in the downloads table, then max(id) will return
  // 0 = kInvalidId, so GetNextDownloadId() will set *id = kInvalidId + 1.
  // If there is at least one record but all of the |id|s are <= kInvalidId,
  // then max(id) will return <= kInvalidId, so GetNextDownloadId should return
  // kInvalidId + 1. Note that any records with |id <= kInvalidId| will be
  // dropped in QueryDownloads()
  // SQLITE doesn't have unsigned integers.
  return 1 + static_cast<uint32>(std::max(
      static_cast<int64>(content::DownloadItem::kInvalidId),
      select_max_id.ColumnInt64(0)));
}

bool DownloadDatabase::DropDownloadTable() {
  return GetDB().Execute("DROP TABLE downloads");
}

void DownloadDatabase::QueryDownloads(
    std::vector<DownloadRow>* results) {
  EnsureInProgressEntriesCleanedUp();

  results->clear();
  std::set<uint32> ids;

  std::map<uint32, DownloadRow*> info_map;

  sql::Statement statement_main(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, current_path, target_path, "
      "mime_type, original_mime_type, "
      "start_time, received_bytes, "
      "total_bytes, state, danger_type, interrupt_reason, end_time, opened, "
      "referrer, by_ext_id, by_ext_name, etag, last_modified "
      "FROM downloads ORDER BY start_time"));

  while (statement_main.Step()) {
    scoped_ptr<DownloadRow> info(new DownloadRow());
    int column = 0;

    // SQLITE does not have unsigned integers, so explicitly handle negative
    // |id|s instead of casting them to very large uint32s, which would break
    // the max(id) logic in GetNextDownloadId().
    int64 signed_id = statement_main.ColumnInt64(column++);
    info->id = static_cast<uint32>(signed_id);
    info->current_path = ColumnFilePath(statement_main, column++);
    info->target_path = ColumnFilePath(statement_main, column++);
    info->mime_type = statement_main.ColumnString(column++);
    info->original_mime_type = statement_main.ColumnString(column++);
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
    info->referrer_url = GURL(statement_main.ColumnString(column++));
    info->by_ext_id = statement_main.ColumnString(column++);
    info->by_ext_name = statement_main.ColumnString(column++);
    info->etag = statement_main.ColumnString(column++);
    info->last_modified = statement_main.ColumnString(column++);

    // If the record is corrupted, note that and drop it.
    // http://crbug.com/251269
    DroppedReason dropped_reason = DROPPED_REASON_MAX;
    if (signed_id <= static_cast<int64>(content::DownloadItem::kInvalidId)) {
      // SQLITE doesn't have unsigned integers.
      dropped_reason = DROPPED_REASON_BAD_ID;
    } else if (!ids.insert(info->id).second) {
      dropped_reason = DROPPED_REASON_DUPLICATE_ID;
      NOTREACHED() << info->id;
    } else if (info->state == DownloadItem::MAX_DOWNLOAD_STATE) {
      dropped_reason = DROPPED_REASON_BAD_STATE;
    } else if (info->danger_type == content::DOWNLOAD_DANGER_TYPE_MAX) {
      dropped_reason = DROPPED_REASON_BAD_DANGER_TYPE;
    }
    if (dropped_reason != DROPPED_REASON_MAX) {
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseRecordDropped",
                                dropped_reason,
                                DROPPED_REASON_MAX + 1);
    } else {
      DCHECK(!ContainsKey(info_map, info->id));
      uint32 id = info->id;
      info_map[id] = info.release();
    }
  }

  sql::Statement statement_chain(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, chain_index, url FROM downloads_url_chains "
      "ORDER BY id, chain_index"));

  while (statement_chain.Step()) {
    int column = 0;
    // See the comment above about SQLITE lacking unsigned integers.
    int64 signed_id = statement_chain.ColumnInt64(column++);
    int chain_index = statement_chain.ColumnInt(column++);

    if (signed_id <= static_cast<int64>(content::DownloadItem::kInvalidId))
      continue;
    uint32 id = static_cast<uint32>(signed_id);

    // Note that these DCHECKs may trip as a result of corrupted databases.
    // We have them because in debug builds the chances are higher there's
    // an actual bug than that the database is corrupt, but we handle the
    // DB corruption case in production code.

    // Confirm the id has already been seen--if it hasn't, discard the
    // record.
    DCHECK(ContainsKey(info_map, id));
    if (!ContainsKey(info_map, id))
      continue;

    // Confirm all previous URLs in the chain have already been seen;
    // if not, fill in with null or discard record.
    int current_chain_size = info_map[id]->url_chain.size();
    std::vector<GURL>* url_chain(&info_map[id]->url_chain);
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

  for (std::map<uint32, DownloadRow*>::iterator
           it = info_map.begin(); it != info_map.end(); ++it) {
    DownloadRow* row = it->second;
    bool empty_url_chain = row->url_chain.empty();
    UMA_HISTOGRAM_BOOLEAN("Download.DatabaseEmptyUrlChain", empty_url_chain);
    if (empty_url_chain) {
      RemoveDownload(row->id);
    } else {
      // Copy the contents of the stored info.
      results->push_back(*row);
    }
    delete row;
    it->second = NULL;
  }
}

bool DownloadDatabase::UpdateDownload(const DownloadRow& data) {
  EnsureInProgressEntriesCleanedUp();

  DCHECK_NE(content::DownloadItem::kInvalidId, data.id);
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
      "SET current_path=?, target_path=?, "
      "mime_type=?, original_mime_type=?, "
      "received_bytes=?, state=?, "
      "danger_type=?, interrupt_reason=?, end_time=?, total_bytes=?, "
      "opened=?, by_ext_id=?, by_ext_name=?, etag=?, last_modified=? "
      "WHERE id=?"));
  int column = 0;
  BindFilePath(statement, data.current_path, column++);
  BindFilePath(statement, data.target_path, column++);
  statement.BindString(column++, data.mime_type);
  statement.BindString(column++, data.original_mime_type);
  statement.BindInt64(column++, data.received_bytes);
  statement.BindInt(column++, state);
  statement.BindInt(column++, danger_type);
  statement.BindInt(column++, static_cast<int>(data.interrupt_reason));
  statement.BindInt64(column++, data.end_time.ToInternalValue());
  statement.BindInt64(column++, data.total_bytes);
  statement.BindInt(column++, (data.opened ? 1 : 0));
  statement.BindString(column++, data.by_ext_id);
  statement.BindString(column++, data.by_ext_name);
  statement.BindString(column++, data.etag);
  statement.BindString(column++, data.last_modified);
  statement.BindInt(column++, data.id);

  return statement.Run();
}

void DownloadDatabase::EnsureInProgressEntriesCleanedUp() {
  if (in_progress_entry_cleanup_completed_)
    return;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE downloads SET state=?, interrupt_reason=? WHERE state=?"));
  statement.BindInt(0, kStateInterrupted);
  statement.BindInt(1, content::DOWNLOAD_INTERRUPT_REASON_CRASH);
  statement.BindInt(2, kStateInProgress);

  statement.Run();
  in_progress_entry_cleanup_completed_ = true;
}

bool DownloadDatabase::CreateDownload(const DownloadRow& info) {
  DCHECK_NE(content::DownloadItem::kInvalidId, info.id);
  EnsureInProgressEntriesCleanedUp();

  if (info.url_chain.empty())
    return false;

  int state = StateToInt(info.state);
  if (state == kStateInvalid)
    return false;

  int danger_type = DangerTypeToInt(info.danger_type);
  if (danger_type == kDangerTypeInvalid)
    return false;

  {
    sql::Statement statement_insert(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO downloads "
        "(id, current_path, target_path, "
        " mime_type, original_mime_type, "
        " start_time, "
        " received_bytes, total_bytes, state, danger_type, interrupt_reason, "
        " end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
        " last_modified) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int column = 0;
    statement_insert.BindInt(column++, info.id);
    BindFilePath(statement_insert, info.current_path, column++);
    BindFilePath(statement_insert, info.target_path, column++);
    statement_insert.BindString(column++, info.mime_type);
    statement_insert.BindString(column++, info.original_mime_type);
    statement_insert.BindInt64(column++, info.start_time.ToInternalValue());
    statement_insert.BindInt64(column++, info.received_bytes);
    statement_insert.BindInt64(column++, info.total_bytes);
    statement_insert.BindInt(column++, state);
    statement_insert.BindInt(column++, danger_type);
    statement_insert.BindInt(column++, info.interrupt_reason);
    statement_insert.BindInt64(column++, info.end_time.ToInternalValue());
    statement_insert.BindInt(column++, info.opened ? 1 : 0);
    statement_insert.BindString(column++, info.referrer_url.spec());
    statement_insert.BindString(column++, info.by_ext_id);
    statement_insert.BindString(column++, info.by_ext_name);
    statement_insert.BindString(column++, info.etag);
    statement_insert.BindString(column++, info.last_modified);
    if (!statement_insert.Run()) {
      // GetErrorCode() returns a bitmask where the lower byte is a more general
      // code and the upper byte is a more specific code. In order to save
      // memory, take the general code, of which there are fewer than 50. See
      // also sql/connection.cc
      // http://www.sqlite.org/c3ref/c_abort_rollback.html
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      return false;
    }
  }

  {
    sql::Statement count_urls(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "SELECT count(*) FROM downloads_url_chains WHERE id=?"));
    count_urls.BindInt(0, info.id);
    if (count_urls.Step()) {
      bool corrupt_urls = count_urls.ColumnInt(0) > 0;
      UMA_HISTOGRAM_BOOLEAN("Download.DatabaseCorruptUrls", corrupt_urls);
      if (corrupt_urls) {
        // There should not be any URLs in downloads_url_chains for this
        // info.id.  If there are, we don't want them to interfere with
        // inserting the correct URLs, so just remove them.
        RemoveDownloadURLs(info.id);
      }
    }
  }

  sql::Statement statement_insert_chain(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO downloads_url_chains "
                                 "(id, chain_index, url) "
                                 "VALUES (?, ?, ?)"));
  for (size_t i = 0; i < info.url_chain.size(); ++i) {
    statement_insert_chain.BindInt(0, info.id);
    statement_insert_chain.BindInt(1, i);
    statement_insert_chain.BindString(2, info.url_chain[i].spec());
    if (!statement_insert_chain.Run()) {
      UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainInsertError",
                                GetDB().GetErrorCode() & 0xff, 50);
      RemoveDownload(info.id);
      return false;
    }
    statement_insert_chain.Reset(true);
  }
  return true;
}

void DownloadDatabase::RemoveDownload(uint32 id) {
  EnsureInProgressEntriesCleanedUp();

  sql::Statement downloads_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads WHERE id=?"));
  downloads_statement.BindInt(0, id);
  if (!downloads_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseMainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
    return;
  }
  RemoveDownloadURLs(id);
}

void DownloadDatabase::RemoveDownloadURLs(uint32 id) {
  sql::Statement urlchain_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM downloads_url_chains WHERE id=?"));
  urlchain_statement.BindInt(0, id);
  if (!urlchain_statement.Run()) {
    UMA_HISTOGRAM_ENUMERATION("Download.DatabaseURLChainDeleteError",
                              GetDB().GetErrorCode() & 0xff, 50);
  }
}

size_t DownloadDatabase::CountDownloads() {
  EnsureInProgressEntriesCleanedUp();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT count(*) from downloads"));
  statement.Step();
  return statement.ColumnInt(0);
}

}  // namespace history
