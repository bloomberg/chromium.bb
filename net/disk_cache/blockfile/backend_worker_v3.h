// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/eviction.h"
#include "net/disk_cache/blockfile/in_flight_backend_io.h"
#include "net/disk_cache/blockfile/rankings.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/blockfile/trace.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

// This class implements the Backend interface. An object of this
// class handles the operations of the cache for a particular profile.
class NET_EXPORT_PRIVATE BackendImpl : public Backend {
  friend class Eviction;
 public:
  BackendImpl(const base::FilePath& path, base::MessageLoopProxy* cache_thread,
              net::NetLog* net_log);

  // Performs general initialization for this current instance of the cache.
  int Init(const CompletionCallback& callback);

 private:
  void CleanupCache();

  // Returns the full name for an external storage file.
  base::FilePath GetFileName(Addr address) const;

  // Creates a new backing file for the cache index.
  bool CreateBackingStore(disk_cache::File* file);
  bool InitBackingStore(bool* file_created);

  // Reports an uncommon, recoverable error.
  void ReportError(int error);

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  base::FilePath path_;  // Path to the folder used as backing storage.
  BlockFiles block_files_;  // Set of files used to store all data.
  bool init_;  // controls the initialization of the system.

  DISALLOW_COPY_AND_ASSIGN(BackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_
