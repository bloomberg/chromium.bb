// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "net/base/net_export.h"

class Pickle;
class PickleIterator;

namespace base {
class SingleThreadTaskRunner;
}

namespace disk_cache {

class NET_EXPORT_PRIVATE EntryMetadata {
 public:
  EntryMetadata();
  EntryMetadata(uint64 hash_key,
                base::Time last_used_time,
                uint64 entry_size);

  uint64 GetHashKey() const { return hash_key_; }
  base::Time GetLastUsedTime() const;
  void SetLastUsedTime(const base::Time& last_used_time);

  uint64 GetEntrySize() const { return entry_size_; }
  void SetEntrySize(uint64 entry_size) { entry_size_ = entry_size; }

  // Serialize the data into the provided pickle.
  void Serialize(Pickle* pickle) const;
  bool Deserialize(PickleIterator* it);

  // Merge two EntryMetadata instances.
  // The existing current valid data in |this| will prevail.
  void MergeWith(const EntryMetadata& entry_metadata);

 private:
  friend class SimpleIndexFileTest;

  // When adding new members here, you should update the Serialize() and
  // Deserialize() methods.
  uint64 hash_key_;

  // This is the serialized format from Time::ToInternalValue().
  // If you want to make calculations/comparisons, you should use the
  // base::Time() class. Use the GetLastUsedTime() method above.
  // TODO(felipeg): Use Time() here.
  int64 last_used_time_;

  uint64 entry_size_;  // Storage size in bytes.
};

// This class is not Thread-safe.
class NET_EXPORT_PRIVATE SimpleIndex
    : public base::SupportsWeakPtr<SimpleIndex> {
 public:
  SimpleIndex(
      base::SingleThreadTaskRunner* cache_thread,
      base::SingleThreadTaskRunner* io_thread,
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

  // TODO(felipeg): This way we are storing the hash_key twice, as the
  // hash_map::key and as a member of EntryMetadata. We could save space if we
  // use a hash_set.
  typedef base::hash_map<uint64, EntryMetadata> EntrySet;

  static void InsertInEntrySet(const EntryMetadata& entry_metadata,
                               EntrySet* entry_set);

 private:
  typedef base::Callback<void(scoped_ptr<EntrySet>)> IndexCompletionCallback;

  static void LoadFromDisk(
      const base::FilePath& index_filename,
      base::SingleThreadTaskRunner* io_thread,
      const IndexCompletionCallback& completion_callback);

  // Enumerates all entries' files on disk and regenerates the index.
  static scoped_ptr<SimpleIndex::EntrySet> RestoreFromDisk(
      const base::FilePath& index_filename);

  static void WriteToDiskInternal(const base::FilePath& index_filename,
                                  scoped_ptr<Pickle> pickle);

  // Must run on IO Thread.
  void MergeInitializingSet(scoped_ptr<EntrySet> index_file_entries);

  EntrySet entries_set_;
  uint64 cache_size_;  // Total cache storage size in bytes.

  // This stores all the hash_key of entries that are removed during
  // initialization.
  base::hash_set<uint64> removed_entries_;
  bool initialized_;

  base::FilePath index_filename_;

  scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  // All nonstatic SimpleEntryImpl methods should always be called on the IO
  // thread, in all cases. |io_thread_checker_| documents and enforces this.
  base::ThreadChecker io_thread_checker_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
