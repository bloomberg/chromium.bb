// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_process_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

MacChromeProcessInfoList GetRunningMacProcessInfo(
    const ChromeProcessList& process_list) {
  MacChromeProcessInfoList result;

  // Build up the ps command line
  std::vector<std::string> cmdline;
  cmdline.push_back("ps");
  cmdline.push_back("-o");
  cmdline.push_back("pid=,rss=,vsz=");  // fields we need, no headings
  ChromeProcessList::const_iterator process_iter;
  for (process_iter = process_list.begin();
       process_iter != process_list.end();
       ++process_iter) {
    cmdline.push_back("-p");
    cmdline.push_back(base::StringPrintf("%d", *process_iter));
  }

  // Invoke it
  std::string ps_output;
  if (!base::GetAppOutput(CommandLine(cmdline), &ps_output))
    return result;  // All the pids might have exited

  // Process the results
  std::vector<std::string> ps_output_lines;
  base::SplitString(ps_output, '\n', &ps_output_lines);
  std::vector<std::string>::const_iterator line_iter;
  for (line_iter = ps_output_lines.begin();
       line_iter != ps_output_lines.end();
       ++line_iter) {
    std::string line(CollapseWhitespaceASCII(*line_iter, false));
    std::vector<std::string> values;
    base::SplitString(line, ' ', &values);
    if (values.size() == 3) {
      MacChromeProcessInfo proc_info;
      int pid;
      base::StringToInt(values[0], &pid);
      proc_info.pid = pid;
      base::StringToInt(values[1], &proc_info.rsz_in_kb);
      base::StringToInt(values[2], &proc_info.vsz_in_kb);
      if (proc_info.pid && proc_info.rsz_in_kb && proc_info.vsz_in_kb)
        result.push_back(proc_info);
    }
  }

  return result;
}

// Common interface for fetching memory values from parsed ps output.
// We fill in both values we may get called for, even though our
// callers typically only care about one, just to keep the code
// simple and because this is a test.
static bool GetMemoryValuesHack(uint32 process_id,
                          size_t* virtual_size,
                          size_t* working_set_size) {
  DCHECK(virtual_size && working_set_size);

  std::vector<base::ProcessId> processes;
  processes.push_back(process_id);

  MacChromeProcessInfoList process_info = GetRunningMacProcessInfo(processes);
  if (process_info.empty())
    return false;

  bool found_process = false;
  *virtual_size = 0;
  *working_set_size = 0;

  MacChromeProcessInfoList::iterator it = process_info.begin();
  for (; it != process_info.end(); ++it) {
    if (it->pid != static_cast<base::ProcessId>(process_id))
      continue;
    found_process = true;
    *virtual_size = it->vsz_in_kb * 1024;
    *working_set_size = it->rsz_in_kb * 1024;
    break;
  }

  return found_process;
}

size_t ChromeTestProcessMetrics::GetPagefileUsage() {
  size_t virtual_size;
  size_t working_set_size;
  GetMemoryValuesHack(process_handle_, &virtual_size, &working_set_size);
  return virtual_size;
}

size_t ChromeTestProcessMetrics::GetWorkingSetSize() {
  size_t virtual_size;
  size_t working_set_size;
  GetMemoryValuesHack(process_handle_, &virtual_size, &working_set_size);
  return working_set_size;
}
