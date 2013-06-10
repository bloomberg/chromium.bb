// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/internal_linux.h"

#include <unistd.h>

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace base {
namespace internal {

const char kProcDir[] = "/proc";

const char kStatFile[] = "stat";

base::FilePath GetProcPidDir(pid_t pid) {
  return base::FilePath(kProcDir).Append(IntToString(pid));
}

pid_t ProcDirSlotToPid(const char* d_name) {
  int i;
  for (i = 0; i < NAME_MAX && d_name[i]; ++i) {
    if (!IsAsciiDigit(d_name[i])) {
      return 0;
    }
  }
  if (i == NAME_MAX)
    return 0;

  // Read the process's command line.
  pid_t pid;
  std::string pid_string(d_name);
  if (!StringToInt(pid_string, &pid)) {
    NOTREACHED();
    return 0;
  }
  return pid;
}

bool ReadProcStats(pid_t pid, std::string* buffer) {
  buffer->clear();
  // Synchronously reading files in /proc is safe.
  ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath stat_file = internal::GetProcPidDir(pid).Append(kStatFile);
  if (!file_util::ReadFileToString(stat_file, buffer)) {
    DLOG(WARNING) << "Failed to get process stats.";
    return false;
  }
  return !buffer->empty();
}

bool ParseProcStats(const std::string& stats_data,
                    std::vector<std::string>* proc_stats) {
  // |stats_data| may be empty if the process disappeared somehow.
  // e.g. http://crbug.com/145811
  if (stats_data.empty())
    return false;

  // The stat file is formatted as:
  // pid (process name) data1 data2 .... dataN
  // Look for the closing paren by scanning backwards, to avoid being fooled by
  // processes with ')' in the name.
  size_t open_parens_idx = stats_data.find(" (");
  size_t close_parens_idx = stats_data.rfind(") ");
  if (open_parens_idx == std::string::npos ||
      close_parens_idx == std::string::npos ||
      open_parens_idx > close_parens_idx) {
    DLOG(WARNING) << "Failed to find matched parens in '" << stats_data << "'";
    NOTREACHED();
    return false;
  }
  open_parens_idx++;

  proc_stats->clear();
  // PID.
  proc_stats->push_back(stats_data.substr(0, open_parens_idx));
  // Process name without parentheses.
  proc_stats->push_back(
      stats_data.substr(open_parens_idx + 1,
                        close_parens_idx - (open_parens_idx + 1)));

  // Split the rest.
  std::vector<std::string> other_stats;
  SplitString(stats_data.substr(close_parens_idx + 2), ' ', &other_stats);
  for (size_t i = 0; i < other_stats.size(); ++i)
    proc_stats->push_back(other_stats[i]);
  return true;
}

int GetProcStatsFieldAsInt(const std::vector<std::string>& proc_stats,
                           ProcStatsFields field_num) {
  DCHECK_GE(field_num, VM_PPID);
  CHECK_LT(static_cast<size_t>(field_num), proc_stats.size());

  int value;
  return StringToInt(proc_stats[field_num], &value) ? value : 0;
}

size_t GetProcStatsFieldAsSizeT(const std::vector<std::string>& proc_stats,
                                ProcStatsFields field_num) {
  DCHECK_GE(field_num, VM_PPID);
  CHECK_LT(static_cast<size_t>(field_num), proc_stats.size());

  size_t value;
  return StringToSizeT(proc_stats[field_num], &value) ? value : 0;
}

int ReadProcStatsAndGetFieldAsInt(pid_t pid,
                                  ProcStatsFields field_num) {
  std::string stats_data;
  if (!ReadProcStats(pid, &stats_data))
    return 0;
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(stats_data, &proc_stats))
    return 0;
  return GetProcStatsFieldAsInt(proc_stats, field_num);
}

size_t ReadProcStatsAndGetFieldAsSizeT(pid_t pid,
                                       ProcStatsFields field_num) {
  std::string stats_data;
  if (!ReadProcStats(pid, &stats_data))
    return 0;
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(stats_data, &proc_stats))
    return 0;
  return GetProcStatsFieldAsSizeT(proc_stats, field_num);
}

}  // namespace internal
}  // namespace base
