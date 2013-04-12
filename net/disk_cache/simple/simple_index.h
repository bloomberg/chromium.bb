// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_disk_format.h"

namespace base {
class TaskRunner;
}

namespace disk_cache {

// This class is not Thread-safe.
class SimpleIndex
    : public base::SupportsWeakPtr<SimpleIndex> {
 public:
  SimpleIndex(
      const scoped_refptr<base::TaskRunner>& cache_thread,
      const scoped_refptr<base::TaskRunner>& io_thread,
      const base::FilePath& path);

  virtual ~SimpleIndex();

  void Initialize();

  void Insert(const std::string& key);
  void Remove(const std::string& key);

  bool Has(const std::string& key) const;

  // Update the last used time of the entry with the given key and return true
  // iff the entry exist in the index.
  bool UseIfExists(const std::string& key);

  void WriteToDisk();

  // Update the size (in bytes) of an entry, in the metadata stored in the
  // index. This should be the total disk-file size including all streams of the
  // entry.
  bool UpdateEntrySize(const std::string& key, uint64 entry_size);

 private:
  // TODO(felipeg): This way we are storing the hash_key string twice (as the
  // hash_map::key and as a member of EntryMetadata. We could save space if we
  // redefine the hash_map::operators and make the hash_map::key be part of the
  // EntryMetadata itself.
  typedef base::hash_map<std::string, SimpleIndexFile::EntryMetadata> EntrySet;

  typedef base::Callback<void(scoped_ptr<EntrySet>)> MergeCallback;

  static void InsertInternal(
      EntrySet* entry_set,
      const SimpleIndexFile::EntryMetadata& entry_metadata);

  // Load index from disk. If it is corrupted, call RestoreFromDisk().
  static void LoadFromDisk(
      const base::FilePath& index_filename,
      const scoped_refptr<base::TaskRunner>& io_thread,
      const MergeCallback& merge_callback);

  // Enumerates all entries' files on disk and regenerates the index.
  static void RestoreFromDisk(
      const base::FilePath& index_filename,
      const scoped_refptr<base::TaskRunner>& io_thread,
      const MergeCallback& merge_callback);

  // Must run on IO Thread.
  void MergeInitializingSet(scoped_ptr<EntrySet> index_file_entries);

  // |out_buffer| needs to be pre-allocated. The serialized index is stored in
  // |out_buffer|.
  void Serialize(std::string* out_buffer);

  bool OpenIndexFile();
  bool CloseIndexFile();

  static void UpdateFile(const base::FilePath& index_filename,
                         const base::FilePath& temp_filename,
                         scoped_ptr<std::string> buffer);

  EntrySet entries_set_;
  uint64 cache_size_;  // Total cache storage size in bytes.

  // This stores all the hash_key of entries that are removed during
  // initialization.
  base::hash_set<std::string> removed_entries_;
  bool initialized_;

  base::FilePath index_filename_;

  scoped_refptr<base::TaskRunner> cache_thread_;
  scoped_refptr<base::TaskRunner> io_thread_;

  // All nonstatic SimpleEntryImpl methods should always be called on the IO
  // thread, in all cases. |io_thread_checker_| documents and enforces this.
  base::ThreadChecker io_thread_checker_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
