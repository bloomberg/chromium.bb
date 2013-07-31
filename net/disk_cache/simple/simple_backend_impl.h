// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_entry_impl.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace disk_cache {

// SimpleBackendImpl is a new cache backend that stores entries in individual
// files.
// See http://www.chromium.org/developers/design-documents/network-stack/disk-cache/very-simple-backend
//
// The non-static functions below must be called on the IO thread unless
// otherwise stated.

class SimpleEntryImpl;
class SimpleIndex;

class NET_EXPORT_PRIVATE SimpleBackendImpl : public Backend,
    public base::SupportsWeakPtr<SimpleBackendImpl> {
 public:
  SimpleBackendImpl(const base::FilePath& path, int max_bytes,
                    net::CacheType type,
                    base::SingleThreadTaskRunner* cache_thread,
                    net::NetLog* net_log);

  virtual ~SimpleBackendImpl();

  SimpleIndex* index() { return index_.get(); }

  base::TaskRunner* worker_pool() { return worker_pool_.get(); }

  int Init(const CompletionCallback& completion_callback);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Returns the maximum file size permitted in this backend.
  int GetMaxFileSize() const;

  // Removes |entry| from the |active_entries_| set, forcing future Open/Create
  // operations to construct a new object.
  void OnDeactivated(const SimpleEntryImpl* entry);

  // Backend:
  virtual net::CacheType GetCacheType() const OVERRIDE;
  virtual int32 GetEntryCount() const OVERRIDE;
  virtual int OpenEntry(const std::string& key, Entry** entry,
                        const CompletionCallback& callback) OVERRIDE;
  virtual int CreateEntry(const std::string& key, Entry** entry,
                          const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntry(const std::string& key,
                        const CompletionCallback& callback) OVERRIDE;
  virtual int DoomAllEntries(const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntriesBetween(base::Time initial_time,
                                 base::Time end_time,
                                 const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntriesSince(base::Time initial_time,
                               const CompletionCallback& callback) OVERRIDE;
  virtual int OpenNextEntry(void** iter, Entry** next_entry,
                            const CompletionCallback& callback) OVERRIDE;
  virtual void EndEnumeration(void** iter) OVERRIDE;
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) OVERRIDE;
  virtual void OnExternalCacheHit(const std::string& key) OVERRIDE;

 private:
  typedef base::hash_map<uint64, base::WeakPtr<SimpleEntryImpl> > EntryMap;

  typedef base::Callback<void(base::Time mtime, uint64 max_size, int result)>
      InitializeIndexCallback;

  // Return value of InitCacheStructureOnDisk().
  struct DiskStatResult {
    base::Time cache_dir_mtime;
    uint64 max_size;
    bool detected_magic_number_mismatch;
    int net_error;
  };

  void InitializeIndex(const CompletionCallback& callback,
                       const DiskStatResult& result);

  // Dooms all entries previously accessed between |initial_time| and
  // |end_time|. Invoked when the index is ready.
  void IndexReadyForDoom(base::Time initial_time,
                         base::Time end_time,
                         const CompletionCallback& callback,
                         int result);

  // Try to create the directory if it doesn't exist. This must run on the IO
  // thread.
  static DiskStatResult InitCacheStructureOnDisk(const base::FilePath& path,
                                                 uint64 suggested_max_size);

  // Searches |active_entries_| for the entry corresponding to |key|. If found,
  // returns the found entry. Otherwise, creates a new entry and returns that.
  scoped_refptr<SimpleEntryImpl> CreateOrFindActiveEntry(
      const std::string& key);

  // Given a hash, will try to open the corresponding Entry. If we have an Entry
  // corresponding to |hash| in the map of active entries, opens it. Otherwise,
  // a new empty Entry will be created, opened and filled with information from
  // the disk.
  int OpenEntryFromHash(uint64 hash,
                        Entry** entry,
                        const CompletionCallback& callback);

  // Called when the index is initilized to find the next entry in the iterator
  // |iter|. If there are no more hashes in the iterator list, net::ERR_FAILED
  // is returned. Otherwise, calls OpenEntryFromHash.
  void GetNextEntryInIterator(void** iter,
                              Entry** next_entry,
                              const CompletionCallback& callback,
                              int error_code);

  // Called when we tried to open an entry with hash alone. When a blank entry
  // has been created and filled in with information from the disk - based on a
  // hash alone - this checks that a duplicate active entry was not created
  // using a key in the meantime.
  void OnEntryOpenedFromHash(uint64 hash,
                             Entry** entry,
                             scoped_refptr<SimpleEntryImpl> simple_entry,
                             const CompletionCallback& callback,
                             int error_code);

  // Called when we tried to open an entry from key. When the entry has been
  // opened, a check for key mismatch is performed.
  void OnEntryOpenedFromKey(const std::string key,
                            Entry** entry,
                            scoped_refptr<SimpleEntryImpl> simple_entry,
                            const CompletionCallback& callback,
                            int error_code);

  // Called at the end of the asynchronous operation triggered by
  // OpenEntryFromHash. Makes sure to continue iterating if the open entry was
  // not a success.
  void CheckIterationReturnValue(void** iter,
                                 Entry** entry,
                                 const CompletionCallback& callback,
                                 int error_code);

  const base::FilePath path_;
  scoped_ptr<SimpleIndex> index_;
  const scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;
  scoped_refptr<base::TaskRunner> worker_pool_;

  int orig_max_size_;
  const SimpleEntryImpl::OperationsMode entry_operations_mode_;

  // TODO(gavinp): Store the entry_hash in SimpleEntryImpl, and index this map
  // by hash. This will save memory, and make IndexReadyForDoom easier.
  EntryMap active_entries_;

  net::NetLog* const net_log_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
