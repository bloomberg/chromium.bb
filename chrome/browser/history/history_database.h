// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_
#define CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_
#pragma once

#include "app/sql/connection.h"
#include "app/sql/init_status.h"
#include "app/sql/meta_table.h"
#include "build/build_config.h"
#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/starred_url_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/history/visitsegment_database.h"

class FilePath;

namespace history {

// Forward declaration for the temporary migration code in Init().
class TextDatabaseManager;

// Encapsulates the SQL connection for the history database. This class holds
// the database connection and has methods the history system (including full
// text search) uses for writing and retrieving information.
//
// We try to keep most logic out of the history database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in HistoryBackend.cc.
class HistoryDatabase : public DownloadDatabase,
  // TODO(sky): See if we can nuke StarredURLDatabase and just create on the
  // stack for migration. Then HistoryDatabase would directly descend from
  // URLDatabase.
                        public StarredURLDatabase,
                        public VisitDatabase,
                        public VisitSegmentDatabase {
 public:
  // A simple class for scoping a history database transaction. This does not
  // support rollback since the history database doesn't, either.
  class TransactionScoper {
   public:
    explicit TransactionScoper(HistoryDatabase* db) : db_(db) {
      db_->BeginTransaction();
    }
    ~TransactionScoper() {
      db_->CommitTransaction();
    }
   private:
    HistoryDatabase* db_;
  };

  // Must call Init() to complete construction. Although it can be created on
  // any thread, it must be destructed on the history thread for proper
  // database cleanup.
  HistoryDatabase();

  virtual ~HistoryDatabase();

  // Must call this function to complete initialization. Will return true on
  // success. On false, no other function should be called. You may want to call
  // BeginExclusiveMode after this when you are ready.
  sql::InitStatus Init(const FilePath& history_name,
                       const FilePath& tmp_bookmarks_path);

  // Call to set the mode on the database to exclusive. The default locking mode
  // is "normal" but we want to run in exclusive mode for slightly better
  // performance since we know nobody else is using the database. This is
  // separate from Init() since the in-memory database attaches to slurp the
  // data out, and this can't happen in exclusive mode.
  void BeginExclusiveMode();

  // Returns the current version that we will generate history databases with.
  static int GetCurrentVersion();

  // Transactions on the history database. Use the Transaction object above
  // for most work instead of these directly. We support nested transactions
  // and only commit when the outermost transaction is committed. This means
  // that it is impossible to rollback a specific transaction. We could roll
  // back the outermost transaction if any inner one is rolled back, but it
  // turns out we don't really need this type of integrity for the history
  // database, so we just don't support it.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {  // for debugging and assertion purposes
    return db_.transaction_nesting();
  }

  // Drops all tables except the URL, and download tables, and recreates them
  // from scratch. This is done to rapidly clean up stuff when deleting all
  // history. It is faster and less likely to have problems that deleting all
  // rows in the tables.
  //
  // We don't delete the downloads table, since there may be in progress
  // downloads. We handle the download history clean up separately in:
  // DownloadManager::RemoveDownloadsFromHistoryBetween.
  //
  // Returns true on success. On failure, the caller should assume that the
  // database is invalid. There could have been an error recreating a table.
  // This should be treated the same as an init failure, and the database
  // should not be used any more.
  //
  // This will also recreate the supplementary URL indices, since these
  // indices won't be created automatically when using the temporary URL
  // table (what the caller does right before calling this).
  bool RecreateAllTablesButURL();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Returns true if the history backend should erase the full text search
  // and archived history files as part of version 16 -> 17 migration. The
  // time format changed in this revision, and these files would be much slower
  // to migrate. Since the data is less important, they should be deleted.
  //
  // This flag will be valid after Init() is called. It will always be false
  // when running on Windows.
  bool needs_version_17_migration() const {
    return needs_version_17_migration_;
  }

  // Marks the database as no longer needing migration.
  void ThumbnailMigrationDone();

  // Returns true if thumbnails needs to be migrated.
  bool GetNeedsThumbnailMigration();

  // Visit table functions ----------------------------------------------------

  // Update the segment id of a visit. Return true on success.
  bool SetSegmentID(VisitID visit_id, SegmentID segment_id);

  // Query the segment ID for the provided visit. Return 0 on failure or if the
  // visit id wasn't found.
  SegmentID GetSegmentID(VisitID visit_id);

  // Retrieves/Updates early expiration threshold, which specifies the earliest
  // known point in history that may possibly to contain visits suitable for
  // early expiration (AUTO_SUBFRAMES).
  virtual base::Time GetEarlyExpirationThreshold();
  virtual void UpdateEarlyExpirationThreshold(base::Time threshold);

 private:
  FRIEND_TEST_ALL_PREFIXES(IconMappingMigrationTest, TestIconMappingMigration);
  // Implemented for URLDatabase.
  virtual sql::Connection& GetDB();

  // Migration -----------------------------------------------------------------

  // Makes sure the version is up-to-date, updating if necessary. If the
  // database is too old to migrate, the user will be notified. In this case, or
  // for other errors, false will be returned. True means it is up-to-date and
  // ready for use.
  //
  // This assumes it is called from the init function inside a transaction. It
  // may commit the transaction and start a new one if migration requires it.
  sql::InitStatus EnsureCurrentVersion(const FilePath& tmp_bookmarks_path);

#if !defined(OS_WIN)
  // Converts the time epoch in the database from being 1970-based to being
  // 1601-based which corresponds to the change in Time.internal_value_.
  void MigrateTimeEpoch();
#endif

  // ---------------------------------------------------------------------------

  sql::Connection db_;
  sql::MetaTable meta_table_;

  base::Time cached_early_expiration_threshold_;

  // See the getters above.
  bool needs_version_17_migration_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDatabase);
};

}  // history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_
