// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/process_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::ProcessEntry;
using base::ProcessId;
using content::BrowserThread;

namespace {

// A helper for |CollectProcessData()| to include the chrome sandboxed
// processes in android which are not running as a child of the browser
// process.
void AddNonChildChromeProcesses(
    std::vector<ProcessMemoryInformation>* processes) {
  base::ProcessIterator process_iter(NULL);
  while (const ProcessEntry* process_entry = process_iter.NextProcessEntry()) {
    const std::vector<std::string>& cmd_args = process_entry->cmd_line_args();
    if (cmd_args.empty() ||
        cmd_args[0].find(chrome::kHelperProcessExecutableName) ==
            std::string::npos) {
      continue;
    }
    ProcessMemoryInformation info;
    info.pid = process_entry->pid();
    processes->push_back(info);
  }
}

// For each of the pids, collect memory information about that process
// and append a record to |out|.
void GetProcessDataMemoryInformation(
    const std::set<ProcessId>& pids, ProcessData* out) {
  for (std::set<ProcessId>::const_iterator i = pids.begin(); i != pids.end();
       ++i) {
    ProcessMemoryInformation pmi;

    pmi.pid = *i;
    pmi.num_processes = 1;

    if (pmi.pid == base::GetCurrentProcId())
      pmi.process_type = content::PROCESS_TYPE_BROWSER;
    else
      pmi.process_type = content::PROCESS_TYPE_UNKNOWN;

    scoped_ptr<base::ProcessMetrics> metrics(
        base::ProcessMetrics::CreateProcessMetrics(*i));
    metrics->GetWorkingSetKBytes(&pmi.working_set);

    out->processes.push_back(pmi);
  }
}

// Find all children of the given process.
void GetAllChildren(const std::vector<ProcessEntry>& processes,
                    const std::set<ProcessId>& roots,
                    std::set<ProcessId>* out) {
  *out = roots;

  std::set<ProcessId> wavefront;
  for (std::set<ProcessId>::const_iterator i = roots.begin(); i != roots.end();
       ++i) {
    wavefront.insert(*i);
  }

  while (wavefront.size()) {
    std::set<ProcessId> next_wavefront;
    for (std::vector<ProcessEntry>::const_iterator i = processes.begin();
         i != processes.end(); ++i) {
      if (wavefront.count(i->parent_pid())) {
        out->insert(i->pid());
        next_wavefront.insert(i->pid());
      }
    }

    wavefront.clear();
    wavefront.swap(next_wavefront);
  }
}

}  // namespace

MemoryDetails::MemoryDetails()
    : user_metrics_mode_(UPDATE_USER_METRICS) {
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& chrome_processes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<ProcessMemoryInformation> all_processes(chrome_processes);
  AddNonChildChromeProcesses(&all_processes);

  std::vector<ProcessEntry> processes;
  base::ProcessIterator process_iter(NULL);
  while (const ProcessEntry* process_entry = process_iter.NextProcessEntry()) {
    processes.push_back(*process_entry);
  }

  std::set<ProcessId> roots;
  roots.insert(base::GetCurrentProcId());
  for (std::vector<ProcessMemoryInformation>::const_iterator
       i = all_processes.begin(); i != all_processes.end(); ++i) {
    roots.insert(i->pid);
  }

  std::set<ProcessId> current_browser_processes;
  GetAllChildren(processes, roots, &current_browser_processes);

  ProcessData current_browser;
  GetProcessDataMemoryInformation(current_browser_processes, &current_browser);
  current_browser.name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  current_browser.process_name =
      reinterpret_cast<unsigned int>(chrome::kBrowserProcessExecutableName);
  process_data_.push_back(current_browser);

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
