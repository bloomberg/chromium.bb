// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/port.h"
#include "net/base/net_export.h"
#include "net/disk_cache/simple/simple_index.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace disk_cache {

const uint64 kSimpleIndexMagicNumber = GG_UINT64_C(0x656e74657220796f);

struct NET_EXPORT_PRIVATE SimpleIndexLoadResult {
  SimpleIndexLoadResult();
  ~SimpleIndexLoadResult();
  void Reset();

  bool did_load;
  SimpleIndex::EntrySet entries;
  bool flush_required;
};

// Simple Index File format is a pickle serialized data of IndexMetadata and
// EntryMetadata objects.  The file format is as follows: one instance of
// serialized |IndexMetadata| followed serialized |EntryMetadata| entries
// repeated |number_of_entries| amount of times.  To know more about the format,
// see SimpleIndexFile::Serialize() and SeeSimpleIndexFile::LoadFromDisk()
// methods.
//
// The non-static methods must run on the IO thread.  All the real
// work is done in the static methods, which are run on the cache thread
// or in worker threads.  Synchronization between methods is the
// responsibility of the caller.
class NET_EXPORT_PRIVATE SimpleIndexFile {
 public:
  class NET_EXPORT_PRIVATE IndexMetadata {
   public:
    IndexMetadata();
    IndexMetadata(uint64 number_of_entries, uint64 cache_size);

    void Serialize(Pickle* pickle) const;
    bool Deserialize(PickleIterator* it);

    bool CheckIndexMetadata();

    uint64 GetNumberOfEntries() { return number_of_entries_; }

   private:
    FRIEND_TEST_ALL_PREFIXES(IndexMetadataTest, Basics);
    FRIEND_TEST_ALL_PREFIXES(IndexMetadataTest, Serialize);

    uint64 magic_number_;
    uint32 version_;
    uint64 number_of_entries_;
    uint64 cache_size_;  // Total cache storage size in bytes.
  };

  SimpleIndexFile(base::SingleThreadTaskRunner* cache_thread,
                  base::TaskRunner* worker_pool,
                  const base::FilePath& cache_directory);
  virtual ~SimpleIndexFile();

  // Get index entries based on current disk context.
  virtual void LoadIndexEntries(base::Time cache_last_modified,
                                const base::Closure& callback,
                                SimpleIndexLoadResult* out_result);

  // Write the specified set of entries to disk.
  virtual void WriteToDisk(const SimpleIndex::EntrySet& entry_set,
                           uint64 cache_size,
                           const base::TimeTicks& start,
                           bool app_on_background);

  // Doom the entries specified in |entry_hashes|, calling |reply_callback|
  // with the result on the current thread when done.
  virtual void DoomEntrySet(scoped_ptr<std::vector<uint64> > entry_hashes,
                            const base::Callback<void(int)>& reply_callback);

 private:
  friend class WrappedSimpleIndexFile;

  // Used for cache directory traversal.
  typedef base::Callback<void (const base::FilePath&)> EntryFileCallback;

  // When loading the entries from disk, add this many extra hash buckets to
  // prevent reallocation on the IO thread when merging in new live entries.
  static const int kExtraSizeForMerge = 512;

  // Synchronous (IO performing) implementation of LoadIndexEntries.
  static void SyncLoadIndexEntries(base::Time cache_last_modified,
                                   const base::FilePath& cache_directory,
                                   const base::FilePath& index_file_path,
                                   SimpleIndexLoadResult* out_result);

  // Load the index file from disk returning an EntrySet. Upon failure, returns
  // NULL.
  static void SyncLoadFromDisk(const base::FilePath& index_filename,
                               SimpleIndexLoadResult* out_result);

  // Returns a scoped_ptr for a newly allocated Pickle containing the serialized
  // data to be written to a file.
  static scoped_ptr<Pickle> Serialize(
      const SimpleIndexFile::IndexMetadata& index_metadata,
      const SimpleIndex::EntrySet& entries);

  // Given the contents of an index file |data| of length |data_len|, returns
  // the corresponding EntrySet. Returns NULL on error.
  static void Deserialize(const char* data, int data_len,
                          SimpleIndexLoadResult* out_result);

  // Implemented either in simple_index_file_posix.cc or
  // simple_index_file_win.cc. base::FileEnumerator turned out to be very
  // expensive in terms of memory usage therefore it's used only on non-POSIX
  // environments for convenience (for now). Returns whether the traversal
  // succeeded.
  static bool TraverseCacheDirectory(
      const base::FilePath& cache_path,
      const EntryFileCallback& entry_file_callback);

  // Scan the index directory for entries, returning an EntrySet of all entries
  // found.
  static void SyncRestoreFromDisk(const base::FilePath& cache_directory,
                                  const base::FilePath& index_file_path,
                                  SimpleIndexLoadResult* out_result);

  // Determines if an index file is stale relative to the time of last
  // modification of the cache directory.
  static bool IsIndexFileStale(base::Time cache_last_modified,
                               const base::FilePath& index_file_path);

  struct PickleHeader : public Pickle::Header {
    uint32 crc;
  };

  const scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;
  const scoped_refptr<base::TaskRunner> worker_pool_;
  const base::FilePath cache_directory_;
  const base::FilePath index_file_;
  const base::FilePath temp_index_file_;

  DISALLOW_COPY_AND_ASSIGN(SimpleIndexFile);
};


}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
