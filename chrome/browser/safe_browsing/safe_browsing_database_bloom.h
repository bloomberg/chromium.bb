// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/lock.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"

namespace base {
  class Time;
}

struct sqlite3;
class SqliteCompiledStatement;
class SqliteStatementCache;
class SQLTransaction;

// The reference implementation database using SQLite.
class SafeBrowsingDatabaseBloom : public SafeBrowsingDatabase {
 public:
  SafeBrowsingDatabaseBloom();
  virtual ~SafeBrowsingDatabaseBloom();

  // SafeBrowsingDatabase interface:
  virtual void Init(const FilePath& filename);
  virtual bool ResetDatabase();
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update);
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks);
  virtual void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes);
  virtual void GetListsInfo(std::vector<SBListChunkRanges>* lists);
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits);
  virtual bool UpdateStarted();
  virtual void UpdateFinished(bool update_succeeded);

 private:
  struct SBPair {
    int chunk_id;
    SBPrefix prefix;
  };

  enum ChunkType {
    ADD_CHUNK = 0,
    SUB_CHUNK = 1,
  };

  // Opens the database.
  bool Open();

  // Closes the database.
  bool Close();

  // Creates the SQL tables.
  bool CreateTables();

  // Checks the database version and if it's incompatible with the current one,
  // resets the database.
  bool CheckCompatibleVersion();

  // Returns true if any of the given prefixes exist for the given host.
  // Also returns the matching list or any prefix matches.
  void CheckUrl(const std::string& host,
                SBPrefix host_key,
                const std::vector<std::string>& paths,
                std::vector<SBPrefix>* prefix_hits);

  // Checks if a chunk is in the database.
  bool ChunkExists(int list_id, ChunkType type, int chunk_id);

  // Return a comma separated list of chunk ids that are in the database for
  // the given list and chunk type.
  void GetChunkIds(int list_id, ChunkType type, std::string* list);

  // Generate a bloom filter.
  virtual void BuildBloomFilter();

  // Helpers for building the bloom filter.
  static int PairCompare(const void* arg1, const void* arg2);

  bool BuildAddPrefixList(SBPair* adds);
  bool BuildAddFullHashCache(HashCache* add_cache);
  bool BuildSubFullHashCache(HashCache* sub_cache);
  bool RemoveSubs(SBPair* adds,
                  std::vector<bool>* adds_removed,
                  HashCache* add_cache,
                  HashCache* sub_cache,
                  int* subs);

  bool UpdateTables();
  bool WritePrefixes(SBPair* adds, const std::vector<bool>& adds_removed,
                     int* new_add_count, scoped_refptr<BloomFilter>* filter);
  void WriteFullHashes(HashCache* hash_cache, bool is_add);
  void WriteFullHashList(const HashList& hash_list, bool is_add);

  // Looks up any cached full hashes we may have.
  void GetCachedFullHashes(const std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update);

  // Remove cached entries that have prefixes contained in the entry.
  bool ClearCachedEntry(SBPrefix, int add_chunk_id, HashCache* hash_cache);

  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Adding add entries to the database.
  void InsertAdd(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertAddPrefix(SBPrefix prefix, int encoded_chunk);
  void InsertAddFullHash(SBPrefix prefix,
                         int encoded_chunk,
                         base::Time received_time,
                         SBFullHash full_prefix);

  // Adding sub entries to the database.
  void InsertSub(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertSubPrefix(SBPrefix prefix,
                       int encoded_chunk,
                       int encoded_add_chunk);
  void InsertSubFullHash(SBPrefix prefix,
                         int encoded_chunk,
                         int encoded_add_chunk,
                         SBFullHash full_prefix,
                         bool use_temp_table);

  // Used for reading full hashes from the database.
  void ReadFullHash(SqliteCompiledStatement* statement,
                    int column,
                    SBFullHash* full_hash);

  // Returns the number of chunk + prefix pairs in the add prefix table.
  int GetAddPrefixCount();

  // Reads and writes chunk numbers to and from persistent store.
  void ReadChunkNumbers();
  bool WriteChunkNumbers();

  // Flush in memory temporary caches.
  void ClearUpdateCaches();

  // Encode the list id in the lower bit of the chunk.
  static inline int EncodeChunkId(int chunk, int list_id) {
    DCHECK(list_id == 0 || list_id == 1);
    chunk = chunk << 1;
    chunk |= list_id;
    return chunk;
  }

  // Split an encoded chunk id and return the original chunk id and list id.
  static inline void DecodeChunkId(int encoded, int* chunk, int* list_id) {
    *list_id = encoded & 0x1;
    *chunk = encoded >> 1;
  }

  // The database connection.
  sqlite3* db_;

  // Cache of compiled statements for our database.
  scoped_ptr<SqliteStatementCache> statement_cache_;

  // Used to schedule resetting the database because of corruption.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseBloom> reset_factory_;

  // Caches for all of the existing add and sub chunks.
  std::set<int> add_chunk_cache_;
  std::set<int> sub_chunk_cache_;

  // Caches for the AddDel and SubDel commands.
  base::hash_set<int> add_del_cache_;
  base::hash_set<int> sub_del_cache_;

  // The number of entries in the add_prefix table. Used to pick the correct
  // size for the bloom filter and stats gathering.
  int add_count_;

  // Transaction for protecting database integrity during updates.
  scoped_ptr<SQLTransaction> insert_transaction_;

  // Lock for protecting access to variables that may be used on the IO thread.
  // This includes |bloom_filter_|, |hash_cache_| and |prefix_miss_cache_|.
  Lock lookup_lock_;

  // True if we're in the middle of a reset.  This is used to prevent possible
  // infinite recursion.
  bool performing_reset_;

  // A store for GetHash results that have not yet been written to the database.
  HashList pending_full_hashes_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseBloom);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_BLOOM_H_
