// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_UTIL_H_
#define NET_DISK_CACHE_CACHE_UTIL_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"

namespace base {
class FilePath;
}

namespace disk_cache {

// Moves the cache files from the given path to another location.
// Fails if the destination exists already, or if it doesn't have
// permission for the operation.  This is basically a rename operation
// for the cache directory.  Returns true if successful.  On ChromeOS,
// this moves the cache contents, and leaves the empty cache
// directory.
NET_EXPORT_PRIVATE bool MoveCache(const base::FilePath& from_path,
                                  const base::FilePath& to_path);

// Deletes the cache files stored on |path|, and optionally also attempts to
// delete the folder itself.
NET_EXPORT_PRIVATE void DeleteCache(const base::FilePath& path,
                                    bool remove_folder);

// Deletes a cache file.
NET_EXPORT_PRIVATE bool DeleteCacheFile(const base::FilePath& name);

// Renames cache directory synchronously and fires off a background cleanup
// task. Used by cache creator itself or by backends for self-restart on error.
bool DelayedCacheCleanup(const base::FilePath& full_path);

// Builds an instance of the backend depending on platform, type, experiments
// etc. Takes care of the retry state. This object will self-destroy when
// finished.
class NET_EXPORT_PRIVATE CacheCreator {
 public:
  CacheCreator(const base::FilePath& path, bool force, int max_bytes,
               net::CacheType type, uint32 flags,
               base::MessageLoopProxy* thread, net::NetLog* net_log,
               disk_cache::Backend** backend,
               const net::CompletionCallback& callback);

  // Creates the backend.
  int Run();

 private:
  ~CacheCreator();

  void DoCallback(int result);

  void OnIOComplete(int result);

  const base::FilePath& path_;
  bool force_;
  bool retry_;
  int max_bytes_;
  net::CacheType type_;
  uint32 flags_;
  scoped_refptr<base::MessageLoopProxy> thread_;
  disk_cache::Backend** backend_;
  net::CompletionCallback callback_;
  disk_cache::Backend* created_cache_;
  net::NetLog* net_log_;
  bool use_simple_cache_backend_;

  DISALLOW_COPY_AND_ASSIGN(CacheCreator);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_UTIL_H_
