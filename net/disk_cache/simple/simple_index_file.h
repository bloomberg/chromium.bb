// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_

#include <string>

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

  typedef base::Callback<void(
      scoped_ptr<SimpleIndex::EntrySet>, bool force_index_flush)>
      IndexCompletionCallback;

  explicit SimpleIndexFile(base::SingleThreadTaskRunner* cache_thread,
                           base::TaskRunner* worker_pool,
                           const base::FilePath& index_file_directory);
  virtual ~SimpleIndexFile();

  // Get index entries based on current disk context.
  virtual void LoadIndexEntries(
      scoped_refptr<base::SingleThreadTaskRunner> response_thread,
      const SimpleIndexFile::IndexCompletionCallback& completion_callback);

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
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, IsIndexFileCorrupt);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, IsIndexFileStale);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, Serialize);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, WriteThenLoadIndex);

  // Using the mtime of the file and its mtime, detects if the index file is
  // stale.
  static bool IsIndexFileStale(const base::FilePath& index_filename);

  // Load the index file from disk, deserializing it and returning the
  // corresponding EntrySet in a scoped_ptr<>, if successful.
  // Uppon failure, the scoped_ptr<> will contain NULL.
  static scoped_ptr<SimpleIndex::EntrySet> LoadFromDisk(
      const base::FilePath& index_filename);

  // Returns a scoped_ptr for a newly allocated Pickle containing the serialized
  // data to be written to a file.
  static scoped_ptr<Pickle> Serialize(
      const SimpleIndexFile::IndexMetadata& index_metadata,
      const SimpleIndex::EntrySet& entries);

  static void LoadIndexEntriesInternal(
      const base::FilePath& index_file_path,
      scoped_refptr<base::SingleThreadTaskRunner> response_thread,
      const SimpleIndexFile::IndexCompletionCallback& completion_callback);

  // Deserialize() is separate from LoadFromDisk() for easier testing.
  static scoped_ptr<SimpleIndex::EntrySet> Deserialize(const char* data,
                                                       int data_len);

  static scoped_ptr<SimpleIndex::EntrySet> RestoreFromDisk(
      const base::FilePath& index_file_path);

  struct PickleHeader : public Pickle::Header {
    uint32 crc;
  };

  const scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;
  const scoped_refptr<base::TaskRunner> worker_pool_;
  const base::FilePath index_file_path_;

  static const char kIndexFileName[];

  DISALLOW_COPY_AND_ASSIGN(SimpleIndexFile);
};


}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
