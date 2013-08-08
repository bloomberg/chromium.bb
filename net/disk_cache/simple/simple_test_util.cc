// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_test_util.h"

#include "base/file_util.h"
#include "net/disk_cache/simple/simple_util.h"

namespace disk_cache {

namespace simple_util {

bool CreateCorruptFileForTests(const std::string& key,
                               const base::FilePath& cache_path) {
  base::FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndIndex(key, 0));
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  base::PlatformFile entry_file =
      base::CreatePlatformFile(entry_file_path, flags, NULL, NULL);

  if (base::kInvalidPlatformFileValue == entry_file)
    return false;
  if (base::WritePlatformFile(entry_file, 0, "dummy", 1) != 1)
    return false;
  if (!base::ClosePlatformFile(entry_file))
    return false;

  return true;
}

}  // namespace simple_backend

}  // namespace disk_cache
