// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <vector>
#include <set>

#include "base/process_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/result_codes.h"

using base::TimeDelta;
using base::TimeTicks;

void TerminateAllChromeProcesses(base::ProcessId browser_pid) {
  ChromeProcessList process_pids(GetRunningChromeProcesses(browser_pid));

  ChromeProcessList::const_iterator it;
  for (it = process_pids.begin(); it != process_pids.end(); ++it) {
    base::ProcessHandle handle;
    if (!base::OpenPrivilegedProcessHandle(*it, &handle)) {
      // Ignore processes for which we can't open the handle. We don't
      // guarantee that all processes will terminate, only try to do so.
      continue;
    }

    base::KillProcess(handle, ResultCodes::TASKMAN_KILL, true);
    base::CloseProcessHandle(handle);
  }
}

class ChildProcessFilter : public base::ProcessFilter {
 public:
  explicit ChildProcessFilter(base::ProcessId parent_pid)
      : parent_pids_(&parent_pid, (&parent_pid) + 1) {}

  explicit ChildProcessFilter(std::vector<base::ProcessId> parent_pids)
      : parent_pids_(parent_pids.begin(), parent_pids.end()) {}

  virtual bool Includes(const base::ProcessEntry& entry) const {
    return parent_pids_.find(entry.parent_pid()) != parent_pids_.end();
  }

 private:
  const std::set<base::ProcessId> parent_pids_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessFilter);
};

ChromeProcessList GetRunningChromeProcesses(base::ProcessId browser_pid) {
  ChromeProcessList result;
  if (browser_pid == static_cast<base::ProcessId>(-1))
    return result;

  ChildProcessFilter filter(browser_pid);
  base::NamedProcessIterator it(chrome::kBrowserProcessExecutableName, &filter);
  while (const base::ProcessEntry* process_entry = it.NextProcessEntry()) {
    result.push_back(process_entry->pid());
  }

#if defined(OS_LINUX)
  // On Linux we might be running with a zygote process for the renderers.
  // Because of that we sweep the list of processes again and pick those which
  // are children of one of the processes that we've already seen.
  {
    ChildProcessFilter filter(result);
    base::NamedProcessIterator it(chrome::kBrowserProcessExecutableName,
                                  &filter);
    while (const base::ProcessEntry* process_entry = it.NextProcessEntry())
      result.push_back(process_entry->pid());
  }
#endif  // defined(OS_LINUX)

#if defined(OS_LINUX) || defined(OS_MACOSX)
  // On Mac OS X we run the subprocesses with a different bundle, and
  // on Linux via /proc/self/exe, so they end up with a different
  // name.  We must collect them in a second pass.
  {
    ChildProcessFilter filter(browser_pid);
    base::NamedProcessIterator it(chrome::kHelperProcessExecutableName,
                                  &filter);
    while (const base::ProcessEntry* process_entry = it.NextProcessEntry())
      result.push_back(process_entry->pid());
  }
#endif  // defined(OS_MACOSX)

  result.push_back(browser_pid);

  return result;
}

#if !defined(OS_MACOSX)

size_t ChromeTestProcessMetrics::GetPagefileUsage() {
  return process_metrics_->GetPagefileUsage();
}

size_t ChromeTestProcessMetrics::GetWorkingSetSize() {
  return process_metrics_->GetWorkingSetSize();
}

#endif  // !defined(OS_MACOSX)
