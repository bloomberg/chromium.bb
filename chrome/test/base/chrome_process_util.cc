// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_process_util.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/time/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/common/result_codes.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

// Returns the executable name of the current Chrome helper process.
std::vector<base::FilePath::StringType> GetRunningHelperExecutableNames() {
  base::FilePath::StringType name = chrome::kHelperProcessExecutableName;

  std::vector<base::FilePath::StringType> names;
  names.push_back(name);

#if defined(OS_MACOSX)
  // The helper might show up as these different flavors depending on the
  // executable flags required.
  for (const char* const* suffix = chrome::kHelperFlavorSuffixes;
       *suffix;
       ++suffix) {
    std::string flavor_name(name);
    flavor_name.append(1, ' ');
    flavor_name.append(*suffix);
    names.push_back(flavor_name);
  }
#endif

  return names;
}

}  // namespace

void TerminateAllChromeProcesses(const ChromeProcessList& process_pids) {
  ChromeProcessList::const_iterator it;
  for (it = process_pids.begin(); it != process_pids.end(); ++it) {
    base::ProcessHandle handle;
    if (!base::OpenProcessHandle(*it, &handle)) {
      // Ignore processes for which we can't open the handle. We don't
      // guarantee that all processes will terminate, only try to do so.
      continue;
    }

    base::KillProcess(handle, content::RESULT_CODE_KILLED, true);
    base::CloseProcessHandle(handle);
  }
}

class ChildProcessFilter : public base::ProcessFilter {
 public:
  explicit ChildProcessFilter(base::ProcessId parent_pid)
      : parent_pids_(&parent_pid, (&parent_pid) + 1) {}

  explicit ChildProcessFilter(const std::vector<base::ProcessId>& parent_pids)
      : parent_pids_(parent_pids.begin(), parent_pids.end()) {}

  virtual bool Includes(const base::ProcessEntry& entry) const OVERRIDE {
    return parent_pids_.find(entry.parent_pid()) != parent_pids_.end();
  }

 private:
  const std::set<base::ProcessId> parent_pids_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessFilter);
};

ChromeProcessList GetRunningChromeProcesses(base::ProcessId browser_pid) {
  const base::FilePath::CharType* executable_name =
      chrome::kBrowserProcessExecutableName;
  ChromeProcessList result;
  if (browser_pid == static_cast<base::ProcessId>(-1))
    return result;

  ChildProcessFilter filter(browser_pid);
  base::NamedProcessIterator it(executable_name, &filter);
  while (const base::ProcessEntry* process_entry = it.NextProcessEntry()) {
    result.push_back(process_entry->pid());
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // On Unix we might be running with a zygote process for the renderers.
  // Because of that we sweep the list of processes again and pick those which
  // are children of one of the processes that we've already seen.
  {
    ChildProcessFilter filter(result);
    base::NamedProcessIterator it(executable_name, &filter);
    while (const base::ProcessEntry* process_entry = it.NextProcessEntry())
      result.push_back(process_entry->pid());
  }
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_POSIX)
  // On Mac OS X we run the subprocesses with a different bundle, and
  // on Linux via /proc/self/exe, so they end up with a different
  // name.  We must collect them in a second pass.
  {
    std::vector<base::FilePath::StringType> names =
        GetRunningHelperExecutableNames();
    for (size_t i = 0; i < names.size(); ++i) {
      base::FilePath::StringType name = names[i];
      ChildProcessFilter filter(browser_pid);
      base::NamedProcessIterator it(name, &filter);
      while (const base::ProcessEntry* process_entry = it.NextProcessEntry())
        result.push_back(process_entry->pid());
    }
  }
#endif  // defined(OS_POSIX)

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

ChromeTestProcessMetrics::~ChromeTestProcessMetrics() {}

ChromeTestProcessMetrics::ChromeTestProcessMetrics(
    base::ProcessHandle process) {
#if !defined(OS_MACOSX)
  process_metrics_.reset(
      base::ProcessMetrics::CreateProcessMetrics(process));
#else
  process_metrics_.reset(
      base::ProcessMetrics::CreateProcessMetrics(process, NULL));
#endif
  process_handle_ = process;
}
