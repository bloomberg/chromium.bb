// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/sync/syncable/dir_open_result.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

extern "C" {
struct sqlite3;
struct sqlite3_stmt;
}

class SQLStatement;

namespace sync_pb {
class EntitySpecifics;
}

namespace syncable {

struct ColumnSpec;
typedef Directory::MetahandlesIndex MetahandlesIndex;

// Provides sqlite3-based persistence for a syncable::Directory object. You can
// load all the persisted data to prime a syncable::Directory on startup by
// invoking Load.  The only other thing you (or more correctly, a Directory)
// can do here is save any changes that have occurred since calling Load, which
// can be done periodically as often as desired*
//
// * If you only ever use a DirectoryBackingStore (DBS) from a single thread
// then you can stop reading now. This is implemented using sqlite3, which
// requires that each thread accesses a DB via a handle (sqlite3*) opened by
// sqlite_open for that thread and only that thread.  To avoid complicated TLS
// logic to swap handles in-and-out as different threads try to get a hold of a
// DBS, the DBS does two things:
// 1.  Uses a separate handle for Load()ing which is closed as soon as loading
//     finishes, and
// 2.  Requires that SaveChanges *only* be called from a single thread, and that
//     thread *must* be the thread that owns / is responsible for destroying
//     the DBS.
// This way, any thread may open a Directory (which today can be either the
// AuthWatcherThread or SyncCoreThread) and Load its DBS.  The first time
// SaveChanges is called a new sqlite3 handle is created, and it will get closed
// when the DBS is destroyed, which is the reason for the requirement that the
// thread that "uses" the DBS is the thread that destroys it.
class DirectoryBackingStore {
 public:
  DirectoryBackingStore(const std::string& dir_name,
                        const FilePath& backing_filepath);

  virtual ~DirectoryBackingStore();

  // Loads and drops all currently persisted meta entries into |entry_bucket|
  // and loads appropriate persisted kernel info into |info_bucket|.
  // NOTE: On success (return value of OPENED), the buckets are populated with
  // newly allocated items, meaning ownership is bestowed upon the caller.
  DirOpenResult Load(MetahandlesIndex* entry_bucket,
                     Directory::KernelLoadInfo* kernel_load_info);

  // Updates the on-disk store with the input |snapshot| as a database
  // transaction.  Does NOT open any syncable transactions as this would cause
  // opening transactions elsewhere to block on synchronous I/O.
  // DO NOT CALL THIS FROM MORE THAN ONE THREAD EVER.  Also, whichever thread
  // calls SaveChanges *must* be the thread that owns/destroys |this|.
  virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot);

 private:
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion67To68);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion68To69);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion69To70);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion70To71);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion71To72);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion72To73);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion73To74);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, MigrateVersion74To75);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, ModelTypeIds);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, Corruption);
  FRIEND_TEST_ALL_PREFIXES(DirectoryBackingStoreTest, DeleteEntries);
  FRIEND_TEST_ALL_PREFIXES(MigrationTest, ToCurrentVersion);
  friend class MigrationTest;

  // General Directory initialization and load helpers.
  DirOpenResult InitializeTables();
  // Returns an sqlite return code, usually SQLITE_DONE.
  int CreateTables();

  // Create 'share_info' or 'temp_share_info' depending on value of
  // is_temporary. Returns an sqlite
  // return code, SQLITE_DONE on success.
  int CreateShareInfoTable(bool is_temporary);

  int CreateShareInfoTableVersion71(bool is_temporary);
  // Create 'metas' or 'temp_metas' depending on value of is_temporary.
  // Returns an sqlite return code, SQLITE_DONE on success.
  int CreateMetasTable(bool is_temporary);
  // Returns an sqlite return code, SQLITE_DONE on success.
  int CreateModelsTable();
  int CreateV71ModelsTable();

  // We don't need to load any synced and applied deleted entries, we can
  // in fact just purge them forever on startup.
  bool DropDeletedEntries();
  // Drops a table if it exists, harmless if the table did not already exist.
  int SafeDropTable(const char* table_name);

  // Load helpers for entries and attributes.
  bool LoadEntries(MetahandlesIndex* entry_bucket);
  bool LoadInfo(Directory::KernelLoadInfo* info);

  // Save/update helpers for entries.  Return false if sqlite commit fails.
  bool SaveEntryToDB(const EntryKernel& entry);
  bool SaveNewEntryToDB(const EntryKernel& entry);
  bool UpdateEntryToDB(const EntryKernel& entry);

  // Creates a new sqlite3 handle to the backing database. Sets sqlite operation
  // timeout preferences and registers our overridden sqlite3 operators for
  // said handle.  Returns true on success, false if the sqlite open operation
  // did not succeed.
  bool OpenAndConfigureHandleHelper(sqlite3** handle) const;
  // Initialize and destroy load_dbhandle_.  Broken out for testing.
  bool BeginLoad();
  void EndLoad();
  DirOpenResult DoLoad(MetahandlesIndex* entry_bucket,
      Directory::KernelLoadInfo* kernel_load_info);

  // Close save_dbhandle_.  Broken out for testing.
  void EndSave();

  // Removes each entry whose metahandle is in |handles| from the database.
  // Does synchronous I/O.  Returns false on error.
  bool DeleteEntries(const MetahandleSet& handles);

  // Lazy creation of save_dbhandle_ for use by SaveChanges code path.
  sqlite3* LazyGetSaveHandle();

  // Drop all tables in preparation for reinitialization.
  void DropAllTables();

  // Serialization helpers for syncable::ModelType.  These convert between
  // the ModelType enum and the values we persist in the database to identify
  // a model.  We persist a default instance of the specifics protobuf as the
  // ID, rather than the enum value.
  static ModelType ModelIdToModelTypeEnum(const void* data, int length);
  static std::string ModelTypeEnumToModelId(ModelType model_type);

  // Runs an integrity check on the current database.  If the
  // integrity check fails, false is returned and error is populated
  // with an error message.
  bool CheckIntegrity(sqlite3* handle, std::string* error) const;

  // Migration utilities.
  bool AddColumn(const ColumnSpec* column);
  bool RefreshColumns();
  bool SetVersion(int version);
  int GetVersion();
  bool MigrateToSpecifics(const char* old_columns,
                          const char* specifics_column,
                          void(*handler_function) (
                              SQLStatement* old_value_query,
                              int old_value_column,
                              sync_pb::EntitySpecifics* mutable_new_value));

  // Individual version migrations.
  bool MigrateVersion67To68();
  bool MigrateVersion68To69();
  bool MigrateVersion69To70();
  bool MigrateVersion70To71();
  bool MigrateVersion71To72();
  bool MigrateVersion72To73();
  bool MigrateVersion73To74();
  bool MigrateVersion74To75();

  // The handle to our sqlite on-disk store for initialization and loading, and
  // for saving changes periodically via SaveChanges, respectively.
  // TODO(timsteele): We should only have one handle here.  The reason we need
  // two at the moment is because the DB can be opened by either the AuthWatcher
  // or SyncCore threads, but SaveChanges is always called by the latter.  We
  // need to change initialization so the DB is only accessed from one thread.
  sqlite3* load_dbhandle_;
  sqlite3* save_dbhandle_;

  std::string dir_name_;
  FilePath backing_filepath_;

  // Set to true if migration left some old columns around that need to be
  // discarded.
  bool needs_column_refresh_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryBackingStore);
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_BACKING_STORE_H_
