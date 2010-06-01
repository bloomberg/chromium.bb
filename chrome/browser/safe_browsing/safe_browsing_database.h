// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_

#include <deque>
#include <list>
#include <set>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"

class BloomFilter;
class GURL;

// Encapsulates the database that stores information about phishing and malware
// sites.  There is one on-disk database for all profiles, as it doesn't
// contain user-specific data.  This object is not thread-safe, i.e. all its
// methods should be used on the same thread that it was created on.
class SafeBrowsingDatabase {
 public:
  // Factory method for obtaining a SafeBrowsingDatabase implementation.
  static SafeBrowsingDatabase* Create();
  virtual ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.
  virtual void Init(const FilePath& filename) = 0;

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() = 0;

  // Returns false if the given url is not in the database.  If it returns
  // true, then either "list" is the name of the matching list, or prefix_hits
  // contains the matching hash prefixes.
  virtual bool ContainsUrl(const GURL& url,
                           std::string* matching_list,
                           std::vector<SBPrefix>* prefix_hits,
                           std::vector<SBFullHashResult>* full_hits,
                           base::Time last_update) = 0;

  // Processes add/sub commands.  Database will free the chunks when it's done.
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks) = 0;

  // Processs adddel/subdel commands.  Database will free chunk_deletes when
  // it's done.
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) = 0;

  // Returns the lists and their add/sub chunks.
  virtual void GetListsInfo(std::vector<SBListChunkRanges>* lists) = 0;

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits) = 0;

  // Returns true if we have successfully started the update transaction.
  virtual bool UpdateStarted() = 0;
  virtual void UpdateFinished(bool update_succeeded) = 0;

  virtual FilePath filename() const { return filename_; }

 protected:
  struct HashCacheEntry {
    SBFullHash full_hash;
    int list_id;
    int add_chunk_id;
    int sub_chunk_id;
    base::Time received;
  };

  typedef std::list<HashCacheEntry> HashList;
  typedef base::hash_map<SBPrefix, HashList> HashCache;

  friend class SafeBrowsingDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, HashCaching);

  static FilePath BloomFilterFilename(const FilePath& db_filename);

  // Load the bloom filter off disk, or generates one if it doesn't exist.
  virtual void LoadBloomFilter();

  // Deletes the on-disk bloom filter, i.e. because it's stale.
  virtual void DeleteBloomFilter();

  // Writes the current bloom filter to disk.
  virtual void WriteBloomFilter();

  // Implementation specific bloom filter building.
  virtual void BuildBloomFilter() = 0;

  scoped_ptr<HashCache> hash_cache_;
  HashCache* hash_cache() { return hash_cache_.get(); }

  // Cache of prefixes that returned empty results (no full hash match).
  typedef std::set<SBPrefix> PrefixCache;
  PrefixCache prefix_miss_cache_;
  PrefixCache* prefix_miss_cache() { return &prefix_miss_cache_; }

  FilePath filename_;
  FilePath bloom_filter_filename_;
  scoped_refptr<BloomFilter> bloom_filter_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
