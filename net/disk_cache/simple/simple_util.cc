// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_util.h"

#include <limits>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "net/disk_cache/simple/simple_disk_format.h"

namespace disk_cache {

namespace simple_util {

std::string GetFilenameFromKeyAndIndex(const std::string& key, int index) {
  return disk_cache::GetEntryHashKeyAsHexString(key) +
      base::StringPrintf("_%1d", index);
}

int32 GetDataSizeFromKeyAndFileSize(const std::string& key, int64 file_size) {
  int64 data_size = file_size - key.size() -
                    sizeof(disk_cache::SimpleFileHeader);
  DCHECK_GE(implicit_cast<int64>(std::numeric_limits<int32>::max()), data_size);
  return data_size;
}

int64 GetFileSizeFromKeyAndDataSize(const std::string& key, int32 data_size) {
  return data_size + key.size() + sizeof(disk_cache::SimpleFileHeader);
}

int64 GetFileOffsetFromKeyAndDataOffset(const std::string& key,
                                        int data_offset) {
  const int64 headers_size = sizeof(disk_cache::SimpleFileHeader) + key.size();
  return headers_size + data_offset;
}

}  // namespace simple_backend

}  // namespace disk_cache
