// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#pragma once

#include <set>
#include <vector>

#include "base/file_path.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/safe_browsing_store.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace base {
  class Time;
}

class BloomFilter;
class GURL;
class MessageLoop;

// Encapsulates the database that stores information about phishing
// and malware sites.  There is one on-disk database for all profiles,
// as it doesn't contain user-specific data.  This object is not
// thread-safe, i.e. all its methods should be used on the same thread
// that it was created on.

class SafeBrowsingDatabase {
 public:
  // Factory method for obtaining a SafeBrowsingDatabase implementation.
  static SafeBrowsingDatabase* Create();
  virtual ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.
  virtual void Init(const FilePath& filename) = 0;

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() = 0;

  // Returns false if |url| is not in the database.  If it returns
  // true, then either |matching_list| is the name of the matching
  // list, or |prefix_hits| and |full_hits| contains the matching hash
  // prefixes.  This function is safe to call from threads other than
  // the creation thread.
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update) = 0;

  // A database transaction should look like:
  //
  // std::vector<SBListChunkRanges> lists;
  // if (db.UpdateStarted(&lists)) {
  //   // Do something with |lists|.
  //
  //   // Process add/sub commands.
  //   db.InsertChunks(list_name, chunks);
  //
  //   // Process adddel/subdel commands.
  //   db.DeleteChunks(chunks_deletes);
  //
  //   // If passed true, processes the collected chunk info and
  //   // rebuilds the bloom filter.  If passed false, rolls everything
  //   // back.
  //   db.UpdateFinished(success);
  // }
  //
  // If UpdateStarted() returns true, the caller MUST eventually call
  // UpdateFinished().  If it returns false, the caller MUST NOT call
  // the other functions.
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) = 0;
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks) = 0;
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) = 0;
  virtual void UpdateFinished(bool update_succeeded) = 0;

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits) = 0;

  // The name of the bloom-filter file for the given database file.
  static FilePath BloomFilterForFilename(const FilePath& db_filename);
};

class SafeBrowsingDatabaseNew : public SafeBrowsingDatabase {
 public:
  // Create a database on the given store.  Takes ownership of
  // |store|.  This method is temporary for
  // SafeBrowsingDatabase::Create(), do not use it otherwise.
  explicit SafeBrowsingDatabaseNew(SafeBrowsingStore* store);

  // Create a database with a default store.
  SafeBrowsingDatabaseNew();

  virtual ~SafeBrowsingDatabaseNew();

  // Implement SafeBrowsingDatabase interface.
  virtual void Init(const FilePath& filename);
  virtual bool ResetDatabase();
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update);
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists);
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks);
  virtual void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes);
  virtual void UpdateFinished(bool update_succeeded);
  virtual void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                                const std::vector<SBFullHashResult>& full_hits);

 private:
  friend class SafeBrowsingDatabaseTest;
  FRIEND_TEST(SafeBrowsingDatabaseTest, HashCaching);

  // Deletes the files on disk.
  bool Delete();

  // Load the bloom filter off disk, or generates one if it doesn't exist.
  void LoadBloomFilter();

  // Writes the current bloom filter to disk.
  void WriteBloomFilter();

  // Helpers for handling database corruption.
  // |OnHandleCorruptDatabase()| runs |ResetDatabase()| and sets
  // |corruption_detected_|, |HandleCorruptDatabase()| posts
  // |OnHandleCorruptDatabase()| to the current thread, to be run
  // after the current task completes.
  // TODO(shess): Wire things up to entirely abort the update
  // transaction when this happens.
  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Helpers for InsertChunks().
  void InsertAdd(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertAddChunks(int list_id, const SBChunkList& chunks);
  void InsertSub(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertSubChunks(int list_id, const SBChunkList& chunks);

  // Used to verify that various calls are made from the thread the
  // object was created on.
  MessageLoop* creation_loop_;

  // Lock for protecting access to variables that may be used on the
  // IO thread.  This includes |bloom_filter_|, |full_hashes_|,
  // |pending_hashes_|, and |prefix_miss_cache_|.
  Lock lookup_lock_;

  // Underlying persistent store for chunk data.
  FilePath filename_;
  scoped_ptr<SafeBrowsingStore> store_;

  // Bloom filter generated from the add-prefixes in |store_|.
  FilePath bloom_filter_filename_;
  scoped_refptr<BloomFilter> bloom_filter_;

  // Cached full-hash items, ordered by prefix for efficient scanning.
  // |full_hashes_| are items from |store_|, |pending_hashes_| are
  // items from |CacheHashResults()|, which will be pushed to the
  // store on the next update.
  std::vector<SBAddFullHash> full_hashes_;
  std::vector<SBAddFullHash> pending_hashes_;

  // Cache of prefixes that returned empty results (no full hash
  // match) to |CacheHashResults()|.  Cached to prevent asking for
  // them every time.  Cleared on next update.
  std::set<SBPrefix> prefix_miss_cache_;

  // Used to schedule resetting the database because of corruption.
  ScopedRunnableMethodFactory<SafeBrowsingDatabaseNew> reset_factory_;

  // Set if corruption is detected during the course of an update.
  // Causes the update functions to fail with no side effects, until
  // the next call to |UpdateStarted()|.
  bool corruption_detected_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
