// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/task_runner.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

// SimpleBackendImpl is a new cache backend that stores entries in individual
// files.

// It is currently a work in progress, missing many features of a real cache,
// such as eviction.

// See http://www.chromium.org/developers/design-documents/network-stack/disk-cache/very-simple-backend

class SimpleIndex;

class NET_EXPORT_PRIVATE SimpleBackendImpl : public Backend {
 public:
  virtual ~SimpleBackendImpl();

  static int CreateBackend(const base::FilePath& full_path,
                           int max_bytes,
                           net::CacheType type,
                           uint32 flags,
                           scoped_refptr<base::TaskRunner> cache_thread,
                           net::NetLog* net_log,
                           Backend** backend,
                           const CompletionCallback& callback);

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
  SimpleBackendImpl(
      const scoped_refptr<base::TaskRunner>& cache_thread,
      const base::FilePath& path);

  // Creates the Cache directory if needed. Performs blocking IO, so it cannot
  // be called on IO thread.
  static void EnsureCachePathExists(
      const base::FilePath& path,
      const scoped_refptr<base::TaskRunner>& cache_thread,
      const scoped_refptr<base::TaskRunner>& io_thread,
      const CompletionCallback& callback,
      Backend** backend);

  // Must run on Cache Thread.
  void Initialize();

  const base::FilePath path_;

  scoped_ptr<SimpleIndex> index_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
