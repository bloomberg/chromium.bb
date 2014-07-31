// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_cpu/cpu_info_provider.h"

#include <cstdio>
#include <sstream>

#include "base/file_util.h"
#include "base/format_macros.h"

namespace extensions {

namespace {

const char kProcStat[] = "/proc/stat";

}  // namespace

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<linked_ptr<api::system_cpu::ProcessorInfo> >* infos) {
  DCHECK(infos);

  std::string contents;
  if (!base::ReadFileToString(base::FilePath(kProcStat), &contents))
     return false;

  std::istringstream iss(contents);
  std::string line;

  // Skip the first line because it is just an aggregated number of
  // all cpuN lines.
  std::getline(iss, line);
  size_t i = 0;
  while (std::getline(iss, line)) {
    if (line.compare(0, 3, "cpu") != 0)
      continue;

    uint64 user = 0, nice = 0, sys = 0, idle = 0;
    int vals = sscanf(line.c_str(),
           "%*s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
           &user, &nice, &sys, &idle);
    DCHECK_EQ(4, vals);

    DCHECK(i < infos->size());
    infos->at(i)->usage.kernel = static_cast<double>(sys);
    infos->at(i)->usage.user = static_cast<double>(user + nice);
    infos->at(i)->usage.idle = static_cast<double>(idle);
    infos->at(i)->usage.total = static_cast<double>(sys + user + nice + idle);
    ++i;
  }
  DCHECK_EQ(infos->size(), i);
  return true;
}

}  // namespace extensions
