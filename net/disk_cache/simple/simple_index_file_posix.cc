// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index_file.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "base/logging.h"

namespace disk_cache {
namespace {

struct DirCloser {
  void operator()(DIR* dir) { closedir(dir); }
};

typedef std::unique_ptr<DIR, DirCloser> ScopedDir;

}  // namespace

// static
bool SimpleIndexFile::TraverseCacheDirectory(
    const base::FilePath& cache_path,
    const EntryFileCallback& entry_file_callback) {
  const ScopedDir dir(opendir(cache_path.value().c_str()));
  if (!dir) {
    PLOG(ERROR) << "opendir " << cache_path.value();
    return false;
  }
  // In all implementations of the C library that Chromium can run with,
  // concurrent calls to readdir that specify different directory streams are
  // thread-safe. This is the case here, since the directory stream is scoped to
  // the current function. See https://codereview.chromium.org/2411833004/#msg3
  errno = 0;
  for (dirent* entry = readdir(dir.get()); entry; entry = readdir(dir.get())) {
    const std::string file_name(entry->d_name);
    if (file_name == "." || file_name == "..")
      continue;
    const base::FilePath file_path = cache_path.Append(
        base::FilePath(file_name));
    entry_file_callback.Run(file_path);
  }
  if (!errno)
    return true;  // The traversal completed successfully.
  PLOG(ERROR) << "readdir " << cache_path.value();
  return false;
}

}  // namespace disk_cache
