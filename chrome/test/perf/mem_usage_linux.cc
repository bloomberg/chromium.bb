// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/mem_usage.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"

namespace {

// The format of /proc/meminfo is:
//
// MemTotal:      8235324 kB
// MemFree:       1628304 kB
// Buffers:        429596 kB
// Cached:        4728232 kB
// ...
const size_t kMemTotalIndex = 1;
const size_t kMemFreeIndex = 4;
const size_t kMemBuffersIndex = 7;
const size_t kMemCacheIndex = 10;

}  // namespace

size_t GetSystemCommitCharge() {
  // Used memory is: total - free - buffers - caches
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!file_util::ReadFileToString(meminfo_file, &meminfo_data))
    return 0;
  std::vector<std::string> meminfo_fields;
  SplitStringAlongWhitespace(meminfo_data, &meminfo_fields);

  if (meminfo_fields.size() < kMemCacheIndex) {
    LOG(ERROR) << "Failed to parse /proc/meminfo.  Only found " <<
      meminfo_fields.size() << " fields.";
    return 0;
  }

  DCHECK_EQ(meminfo_fields[kMemTotalIndex-1], "MemTotal:");
  DCHECK_EQ(meminfo_fields[kMemFreeIndex-1], "MemFree:");
  DCHECK_EQ(meminfo_fields[kMemBuffersIndex-1], "Buffers:");
  DCHECK_EQ(meminfo_fields[kMemCacheIndex-1], "Cached:");

  int result_in_kb;
  result_in_kb = StringToInt(meminfo_fields[kMemTotalIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemFreeIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemBuffersIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemCacheIndex]);

  return result_in_kb * 1024;
}

void PrintChromeMemoryUsageInfo() {
  NOTIMPLEMENTED();
}

