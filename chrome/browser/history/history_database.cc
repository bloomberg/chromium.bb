// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_database.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_field_trial.h"
#include "chrome/browser/history/starred_url_database.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace history {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// or database without *too* many bad effects.
static const int kCurrentVersionNumber = 20;
static const int kCompatibleVersionNumber = 16;
static const char kEarlyExpirationThresholdKey[] = "early_expiration_threshold";

// Key in the meta table used to determine if we need to migrate thumbnails out
// of history.
static const char kNeedsThumbnailMigrationKey[] = "needs_thumbnail_migration";

void ComputeDatabaseMetrics(const FilePath& history_name,
                            sql::Connection& db) {
  if (base::RandInt(1, 100) != 50)
    return;  // Only do this computation sometimes since it can be expensive.

  int64 file_size = 0;
  if (!file_util::GetFileSize(history_name, &file_size))
    return;
  int file_mb = static_cast<int>(file_size / (1024 * 1024));
  UMA_HISTOGRAM_MEMORY_MB("History.DatabaseFileMB", file_mb);

  sql::Statement url_count(db.GetUniqueStatement("SELECT count(*) FROM urls"));
  if (!url_count || !url_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS("History.URLTableCount", url_count.ColumnInt(0));

  sql::Statement visit_count(db.GetUniqueStatement(
      "SELECT count(*) FROM visits"));
  if (!visit_count || !visit_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS("History.VisitTableCount", visit_count.ColumnInt(0));
}

}  // namespace

HistoryDatabase::HistoryDatabase()
    : needs_version_17_migration_(false) {
}

HistoryDatabase::~HistoryDatabase() {
}

sql::InitStatus HistoryDatabase::Init(const FilePath& history_name,
                                      const FilePath& bookmarks_path) {
  // Set the exceptional sqlite error handler.
  db_.set_error_delegate(GetErrorHandlerForHistoryDb());

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  if (HistoryFieldTrial::IsLowMemFieldTrial()) {
    db_.set_cache_size(500);
  } else {
    // Increase the cache size. The page size, plus a little extra, times this
    // value, tells us how much memory the cache will use maximum.
    // 6000 * 4MB = 24MB
    // TODO(brettw) scale this value to the amount of available memory.
    db_.set_cache_size(6000);
  }

  // Note that we don't set exclusive locking here. That's done by
  // BeginExclusiveMode below which is called later (we have to be in shared
  // mode to start out for the in-memory backend to read the data).

  if (!db_.Open(history_name))
    return sql::INIT_FAILURE;

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

#if defined(OS_MACOSX)
  // Exclude the history file from backups.
  base::mac::SetFileBackupExclusion(history_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable() || !InitDownloadTable() ||
      !InitSegmentTables())
    return sql::INIT_FAILURE;
  CreateMainURLIndex();
  CreateKeywordSearchTermsIndices();

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion(bookmarks_path);
  if (version_status != sql::INIT_OK)
    return version_status;

  ComputeDatabaseMetrics(history_name, db_);
  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

void HistoryDatabase::BeginExclusiveMode() {
  // We can't use set_exclusive_locking() since that only has an effect before
  // the DB is opened.
  db_.Execute("PRAGMA locking_mode=EXCLUSIVE");
}

// static
int HistoryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void HistoryDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void HistoryDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

bool HistoryDatabase::RecreateAllTablesButURL() {
  if (!DropVisitTable())
    return false;
  if (!InitVisitTable())
    return false;

  if (!DropKeywordSearchTermsTable())
    return false;
  if (!InitKeywordSearchTermsTable())
    return false;

  if (!DropSegmentTables())
    return false;
  if (!InitSegmentTables())
    return false;

  // We also add the supplementary URL indices at this point. This index is
  // over parts of the URL table that weren't automatically created when the
  // temporary URL table was
  CreateKeywordSearchTermsIndices();
  return true;
}

void HistoryDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting()) <<
      "Can not have a transaction when vacuuming.";
  db_.Execute("VACUUM");
}

void HistoryDatabase::ThumbnailMigrationDone() {
  meta_table_.SetValue(kNeedsThumbnailMigrationKey, 0);
}

bool HistoryDatabase::GetNeedsThumbnailMigration() {
  int value = 0;
  return (meta_table_.GetValue(kNeedsThumbnailMigrationKey, &value) &&
          value != 0);
}

bool HistoryDatabase::SetSegmentID(VisitID visit_id, SegmentID segment_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET segment_id = ? WHERE id = ?"));
  if (!s) {
    NOTREACHED() << db_.GetErrorMessage();
    return false;
  }
  s.BindInt64(0, segment_id);
  s.BindInt64(1, visit_id);
  DCHECK(db_.GetLastChangeCount() == 1);
  return s.Run();
}

SegmentID HistoryDatabase::GetSegmentID(VisitID visit_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id FROM visits WHERE id = ?"));
  if (!s) {
    NOTREACHED() << db_.GetErrorMessage();
    return 0;
  }

  s.BindInt64(0, visit_id);
  if (s.Step()) {
    if (s.ColumnType(0) == sql::COLUMN_TYPE_NULL)
      return 0;
    else
      return s.ColumnInt64(0);
  }
  return 0;
}

base::Time HistoryDatabase::GetEarlyExpirationThreshold() {
  if (!cached_early_expiration_threshold_.is_null())
    return cached_early_expiration_threshold_;

  int64 threshold;
  if (!meta_table_.GetValue(kEarlyExpirationThresholdKey, &threshold)) {
    // Set to a very early non-zero time, so it's before all history, but not
    // zero to avoid re-retrieval.
    threshold = 1L;
  }

  cached_early_expiration_threshold_ = base::Time::FromInternalValue(threshold);
  return cached_early_expiration_threshold_;
}

void HistoryDatabase::UpdateEarlyExpirationThreshold(base::Time threshold) {
  meta_table_.SetValue(kEarlyExpirationThresholdKey,
                       threshold.ToInternalValue());
  cached_early_expiration_threshold_ = threshold;
}

sql::Connection& HistoryDatabase::GetDB() {
  return db_;
}

// Migration -------------------------------------------------------------------

sql::InitStatus HistoryDatabase::EnsureCurrentVersion(
    const FilePath& tmp_bookmarks_path) {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "History database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // NOTICE: If you are changing structures for things shared with the archived
  // history file like URLs, visits, or downloads, that will need migration as
  // well. Instead of putting such migration code in this class, it should be
  // in the corresponding file (url_database.cc, etc.) and called from here and
  // from the archived_database.cc.

  int cur_version = meta_table_.GetVersionNumber();

  // Put migration code here

  if (cur_version == 15) {
    StarredURLDatabase starred_url_database(&db_);
    if (!starred_url_database.MigrateBookmarksToFile(tmp_bookmarks_path) ||
        !DropStarredIDFromURLs()) {
      LOG(WARNING) << "Unable to update history database to version 16.";
      return sql::INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 16) {
#if !defined(OS_WIN)
    // In this version we bring the time format on Mac & Linux in sync with the
    // Windows version so that profiles can be moved between computers.
    MigrateTimeEpoch();
#endif
    // On all platforms we bump the version number, so on Windows this
    // migration is a NOP. We keep the compatible version at 16 since things
    // will basically still work, just history will be in the future if an
    // old version reads it.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 17) {
    // Version 17 was for thumbnails to top sites migration. We ended up
    // disabling it though, so 17->18 does nothing.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 18) {
    // This is the version prior to adding url_source column. We need to
    // migrate the database.
    cur_version = 19;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 19) {
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
    // Set a key indicating we need to migrate thumbnails. When successfull the
    // key is removed (ThumbnailMigrationDone).
    meta_table_.SetValue(kNeedsThumbnailMigrationKey, 1);
  }

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < GetCurrentVersion()) <<
         "History database version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}

#if !defined(OS_WIN)
void HistoryDatabase::MigrateTimeEpoch() {
  // Update all the times in the URLs and visits table in the main database.
  // For visits, clear the indexed flag since we'll delete the FTS databases in
  // the next step.
  db_.Execute(
      "UPDATE urls "
      "SET last_visit_time = last_visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM urls WHERE last_visit_time > 0);");
  db_.Execute(
      "UPDATE visits "
      "SET visit_time = visit_time + 11644473600000000, is_indexed = 0 "
      "WHERE id IN (SELECT id FROM visits WHERE visit_time > 0);");
  db_.Execute(
      "UPDATE segment_usage "
      "SET time_slot = time_slot + 11644473600000000 "
      "WHERE id IN (SELECT id FROM segment_usage WHERE time_slot > 0);");

  // Erase all the full text index files. These will take a while to update and
  // are less important, so we just blow them away. Same with the archived
  // database.
  needs_version_17_migration_ = true;
}
#endif

}  // namespace history
