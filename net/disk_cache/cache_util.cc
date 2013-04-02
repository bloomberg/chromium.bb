// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include "base/file_util.h"
#include "base/location.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"

namespace {

const int kMaxOldFolders = 100;

// Returns a fully qualified name from path and name, using a given name prefix
// and index number. For instance, if the arguments are "/foo", "bar" and 5, it
// will return "/foo/old_bar_005".
base::FilePath GetPrefixedName(const base::FilePath& path,
                               const std::string& name,
                               int index) {
  std::string tmp = base::StringPrintf("%s%s_%03d", "old_",
                                       name.c_str(), index);
  return path.AppendASCII(tmp);
}

// This is a simple callback to cleanup old caches.
void CleanupCallback(const base::FilePath& path, const std::string& name) {
  for (int i = 0; i < kMaxOldFolders; i++) {
    base::FilePath to_delete = GetPrefixedName(path, name, i);
    disk_cache::DeleteCache(to_delete, true);
  }
}

// Returns a full path to rename the current cache, in order to delete it. path
// is the current folder location, and name is the current folder name.
base::FilePath GetTempCacheName(const base::FilePath& path,
                                const std::string& name) {
  // We'll attempt to have up to kMaxOldFolders folders for deletion.
  for (int i = 0; i < kMaxOldFolders; i++) {
    base::FilePath to_delete = GetPrefixedName(path, name, i);
    if (!file_util::PathExists(to_delete))
      return to_delete;
  }
  return base::FilePath();
}

}  // namespace

namespace disk_cache {

// In order to process a potentially large number of files, we'll rename the
// cache directory to old_ + original_name + number, (located on the same parent
// directory), and use a worker thread to delete all the files on all the stale
// cache directories. The whole process can still fail if we are not able to
// rename the cache directory (for instance due to a sharing violation), and in
// that case a cache for this profile (on the desired path) cannot be created.
bool DelayedCacheCleanup(const base::FilePath& full_path) {
  // GetTempCacheName() and MoveCache() use synchronous file
  // operations.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  base::FilePath current_path = full_path.StripTrailingSeparators();

  base::FilePath path = current_path.DirName();
  base::FilePath name = current_path.BaseName();
#if defined(OS_POSIX)
  std::string name_str = name.value();
#elif defined(OS_WIN)
  // We created this file so it should only contain ASCII.
  std::string name_str = WideToASCII(name.value());
#endif

  base::FilePath to_delete = GetTempCacheName(path, name_str);
  if (to_delete.empty()) {
    LOG(ERROR) << "Unable to get another cache folder";
    return false;
  }

  if (!disk_cache::MoveCache(full_path, to_delete)) {
    LOG(ERROR) << "Unable to move cache folder " << full_path.value() << " to "
               << to_delete.value();
    return false;
  }

  base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&CleanupCallback, path, name_str), true);
  return true;
}

}  // namespace disk_cache
