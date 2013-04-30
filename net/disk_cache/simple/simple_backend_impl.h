// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace disk_cache {

// SimpleBackendImpl is a new cache backend that stores entries in individual
// files.

// It is currently a work in progress, missing many features of a real cache,
// such as eviction.

// See http://www.chromium.org/developers/design-documents/network-stack/disk-cache/very-simple-backend

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

  // Must run on IO Thread.
  int Init(const CompletionCallback& completion_callback);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Removes |entry| from the |active_entries_| set, forcing future Open/Create
  // operations to construct a new object.
  void OnDeactivated(const SimpleEntryImpl* entry);

  // From Backend:
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
  typedef std::map<uint64, base::WeakPtr<SimpleEntryImpl> > EntryMap;

  typedef base::Callback<void(uint64 max_size, int result)>
      InitializeIndexCallback;

  // Must run on IO Thread.
  void InitializeIndex(const CompletionCallback& callback,
                       uint64 suggested_max_size,
                       int result);

  // Dooms all entries previously accessed between |initial_time| and
  // |end_time|. Invoked when the index is ready.
  void IndexReadyForDoom(base::Time initial_time,
                         base::Time end_time,
                         const CompletionCallback& callback,
                         int result);

  // Try to create the directory if it doesn't exist. Replies with maximum cache
  // size adjustment. Must run on Cache Thread.
  static void ProvideDirectorySuggestBetterCacheSize(
      base::SingleThreadTaskRunner* io_thread,
      const base::FilePath& path,
      const InitializeIndexCallback& initialize_index_callback,
      uint64 suggested_max_size);

  // Searches |active_entries_| for the entry corresponding to |key|. If found,
  // returns the found entry. Otherwise, creates a new entry and returns that.
  scoped_refptr<SimpleEntryImpl> CreateOrFindActiveEntry(
      const std::string& key);

  const base::FilePath path_;
  scoped_ptr<SimpleIndex> index_;
  const scoped_refptr<base::SingleThreadTaskRunner> cache_thread_;

  int orig_max_size_;

  // TODO(gavinp): Store the entry_hash in SimpleEntryImpl, and index this map
  // by hash. This will save memory, and make IndexReadyForDoom easier.
  EntryMap active_entries_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
