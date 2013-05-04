// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

#if defined(OS_ANDROID)
#include "base/android/activity_status.h"
#endif

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
  SimpleIndex(base::SingleThreadTaskRunner* cache_thread,
              base::SingleThreadTaskRunner* io_thread,
              const base::FilePath& path);

  virtual ~SimpleIndex();

  void Initialize();

  bool SetMaxSize(int max_bytes);

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

  // Executes the |callback| when the index is ready. Allows multiple callbacks.
  int ExecuteWhenReady(const net::CompletionCallback& callback);

  // Takes out entries from the index that have last accessed time matching the
  // range between |initial_time| and |end_time| where open intervals are
  // possible according to the definition given in |DoomEntriesBetween()| in the
  // disk cache backend interface. Returns the set of hashes taken out.
  scoped_ptr<std::vector<uint64> > RemoveEntriesBetween(
      const base::Time initial_time,
      const base::Time end_time);

  // Returns number of indexed entries.
  int32 GetEntryCount() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, IsIndexFileStale);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, IndexSizeCorrectOnMerge);
  typedef base::Callback<void(scoped_ptr<EntrySet>, bool force_index_flush)>
      IndexCompletionCallback;

  void StartEvictionIfNeeded();
  void EvictionDone(scoped_ptr<int> result);

  void PostponeWritingToDisk();

  // Using the mtime of the file and its mtime, detects if the index file is
  // stale.
  static bool IsIndexFileStale(const base::FilePath& index_filename);

  static void InitializeInternal(
      const base::FilePath& index_filename,
      base::SingleThreadTaskRunner* io_thread,
      const IndexCompletionCallback& completion_callback);

  // Enumerates all entries' files on disk and regenerates the index.
  static scoped_ptr<SimpleIndex::EntrySet> RestoreFromDisk(
      const base::FilePath& index_filename);

  static void WriteToDiskInternal(const base::FilePath& index_filename,
                                  scoped_ptr<Pickle> pickle,
                                  const base::TimeTicks& start_time);

  // Must run on IO Thread.
  void MergeInitializingSet(scoped_ptr<EntrySet> index_file_entries,
                            bool force_index_flush);

#if defined(OS_ANDROID)
  void OnActivityStateChange(base::android::ActivityState state);

  scoped_ptr<base::android::ActivityStatus::Listener> activity_status_listener_;
#endif

  EntrySet entries_set_;

  uint64 cache_size_;  // Total cache storage size in bytes.
  uint64 max_size_;
  uint64 high_watermark_;
  uint64 low_watermark_;
  bool eviction_in_progress_;
  base::TimeTicks eviction_start_time_;

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

  // Timestamp of the last time we wrote the index to disk.
  // PostponeWritingToDisk() may give up postponing and allow the write if it
  // has been a while since last time we wrote.
  base::Time last_write_to_disk_;
  base::OneShotTimer<SimpleIndex> write_to_disk_timer_;

  typedef std::list<net::CompletionCallback> CallbackList;
  CallbackList to_run_when_initialized_;

  // Set to true when the app is on the background. When the app is in the
  // background we can write the index much more frequently, to insure fresh
  // index on next startup.
  bool app_on_background_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
