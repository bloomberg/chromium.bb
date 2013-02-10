// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include <cstdio>
#include <sstream>

#include "base/file_util.h"
#include "base/format_macros.h"

namespace extensions {

namespace {

const char kProcStat[] = "/proc/stat";

}  // namespace

bool CpuInfoProvider::QueryCpuTimePerProcessor(std::vector<CpuTime>* times) {
  DCHECK(times);

  std::string contents;
  if (!file_util::ReadFileToString(base::FilePath(kProcStat), &contents))
     return false;

  std::istringstream iss(contents);
  uint64 user = 0, nice = 0, sys = 0, idle = 0;
  std::string line;

  // Skip the first line because it is just an aggregated number of
  // all cpuN lines.
  std::getline(iss, line);
  std::vector<CpuTime> results;
  while (std::getline(iss, line)) {
    if (line.compare(0, 3, "cpu") != 0)
      continue;

    sscanf(line.c_str(), "%*s %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64,
           &user, &nice, &sys, &idle);

    CpuTime time;
    time.kernel = sys;
    time.user = user + nice;
    time.idle = idle;
    results.push_back(time);
  }
  times->swap(results);
  return true;
}

}  // namespace extensions
