// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_SQLITE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_SQLITE_H_
#pragma once

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_store.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

struct sqlite3;
class SqliteStatementCache;
class SQLTransaction;

class SafeBrowsingStoreSqlite : public SafeBrowsingStore {
 public:
  SafeBrowsingStoreSqlite();
  virtual ~SafeBrowsingStoreSqlite();

  virtual bool Delete();

  virtual void Init(const FilePath& filename,
                    Callback0::Type* corruption_callback);

  virtual bool BeginChunk();

  // Get all Add prefixes out from the store.
  virtual bool GetAddPrefixes(std::vector<SBAddPrefix>* add_prefixes);
  virtual bool WriteAddPrefix(int32 chunk_id, SBPrefix prefix);
  virtual bool WriteAddHash(int32 chunk_id,
                            base::Time receive_time,
                            const SBFullHash& full_hash);
  virtual bool WriteSubPrefix(int32 chunk_id,
                              int32 add_chunk_id, SBPrefix prefix);
  virtual bool WriteSubHash(int32 chunk_id, int32 add_chunk_id,
                            const SBFullHash& full_hash);
  virtual bool FinishChunk();

  virtual bool BeginUpdate();
  // TODO(shess): Should not be public.
  virtual bool DoUpdate(const std::vector<SBAddFullHash>& pending_adds,
                        std::vector<SBAddPrefix>* add_prefixes_result,
                        std::vector<SBAddFullHash>* add_full_hashes_result);
  // NOTE: |prefix_misses| is ignored, as it will be handled in
  // |SafeBrowsingStoreFile::DoUpdate()|.
  virtual bool FinishUpdate(const std::vector<SBAddFullHash>& pending_adds,
                            const std::set<SBPrefix>& prefix_misses,
                            std::vector<SBAddPrefix>* add_prefixes_result,
                            std::vector<SBAddFullHash>* add_full_hashes_result);
  virtual bool CancelUpdate();

  virtual void SetAddChunk(int32 chunk_id);
  virtual bool CheckAddChunk(int32 chunk_id);
  virtual void GetAddChunks(std::vector<int32>* out);

  virtual void SetSubChunk(int32 chunk_id);
  virtual bool CheckSubChunk(int32 chunk_id);
  virtual void GetSubChunks(std::vector<int32>* out);

  virtual void DeleteAddChunk(int32 chunk_id);
  virtual void DeleteSubChunk(int32 chunk_id);

  // Returns the name of the SQLite journal file for |filename|.
  // Exported for unit tests.
  static const FilePath JournalFileForFilename(const FilePath& filename);

 private:
  // For on-the-fly migration.
  // TODO(shess): Remove (entire class) after migration.
  friend class SafeBrowsingStoreFile;

  // The following routines return true on success, or false on
  // failure.  Failure is presumed to be persistent, so the caller
  // should stop trying and unwind the transaction.
  // OnCorruptDatabase() is called if SQLite returns SQLITE_CORRUPT.

  // Open |db_| from |filename_|, creating if necessary.
  bool Open();

  // Close |db_|, rolling back any in-progress transaction.
  bool Close();

  // Execute all statements in sql, returning true if every one of
  // them returns SQLITE_OK.
  bool ExecSql(const char* sql);

  bool SetupDatabase();
  bool CheckCompatibleVersion();

  bool CreateTables();

  // Clear the old safe-browsing data from the tables.
  bool ResetTables();

  // Read and write the chunks-seen data from |*_chunks_cache_|.
  // Chunk deletions are not accounted for.
  bool ReadAddChunks();
  bool ReadSubChunks();
  bool WriteAddChunks();
  bool WriteSubChunks();

  // Read the various types of data, skipping items which belong to
  // deleted chunks.  New data is appended to the vectors.
  bool ReadAddPrefixes(std::vector<SBAddPrefix>* add_prefixes);
  bool ReadSubPrefixes(std::vector<SBSubPrefix>* sub_prefixes);
  bool ReadAddHashes(std::vector<SBAddFullHash>* add_hashes);
  bool ReadSubHashes(std::vector<SBSubFullHash>* sub_hashes);

  // Write the various types of data.  The existing data is not
  // cleared.
  bool WriteAddPrefixes(const std::vector<SBAddPrefix>& add_prefixes);
  bool WriteSubPrefixes(const std::vector<SBSubPrefix>& sub_prefixes);
  bool WriteAddHashes(const std::vector<SBAddFullHash>& add_hashes);
  bool WriteSubHashes(const std::vector<SBSubFullHash>& sub_hashes);

  // Calls |corruption_callback_| if non-NULL, always returns false as
  // a convenience to the caller.
  bool OnCorruptDatabase();

  // The database path from Init().
  FilePath filename_;

  // Between BeginUpdate() and FinishUpdate(), this will be the SQLite
  // database connection.  Otherwise NULL.
  sqlite3 *db_;

  // Cache of compiled statements for |db_|.
  // TODO(shess): Probably doesn't gain us much.
  scoped_ptr<SqliteStatementCache> statement_cache_;

  // Transaction for protecting database integrity between
  // BeginUpdate() and FinishUpdate().
  scoped_ptr<SQLTransaction> insert_transaction_;

  // The set of chunks which the store has seen.  Elements are added
  // by SetAddChunk() and SetSubChunk(), and deleted on write for
  // chunks that have been deleted.
  std::set<int32> add_chunks_cache_;
  std::set<int32> sub_chunks_cache_;

  // Cache the DeletedAddChunk() and DeleteSubChunk() chunks for later
  // use in FinishUpdate().
  base::hash_set<int32> add_del_cache_;
  base::hash_set<int32> sub_del_cache_;

  // Called when SQLite returns SQLITE_CORRUPT.
  scoped_ptr<Callback0::Type> corruption_callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingStoreSqlite);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_STORE_SQLITE_H_
