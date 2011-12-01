// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>

#include "base/bind.h"
#include "base/eintr_wrapper.h"
#include "base/file_version_info.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/zygote_host_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "grit/chromium_strings.h"

using content::BrowserThread;

// Known browsers which we collect details for.
enum BrowserType {
  CHROME = 0,
  FIREFOX,
  ICEWEASEL,
  OPERA,
  KONQUEROR,
  EPIPHANY,
  MIDORI,
  MAX_BROWSERS
} BrowserProcess;

// The pretty printed names of those browsers. Matches up with enum
// BrowserType.
static const char kBrowserPrettyNames[][10] = {
  "Chrome",
  "Firefox",
  "Iceweasel",
  "Opera",
  "Konqueror",
  "Epiphany",
  "Midori",
};

// A mapping from process name to the type of browser.
static const struct {
  const char process_name[16];
  BrowserType browser;
  } kBrowserBinaryNames[] = {
  { "firefox", FIREFOX },
  { "firefox-3.5", FIREFOX },
  { "firefox-3.0", FIREFOX },
  { "firefox-bin", FIREFOX },
  { "iceweasel", ICEWEASEL },
  { "opera", OPERA },
  { "konqueror", KONQUEROR },
  { "epiphany-browse", EPIPHANY },
  { "epiphany", EPIPHANY },
  { "midori", MIDORI },
  { "", MAX_BROWSERS },
};

MemoryDetails::MemoryDetails() {
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

struct Process {
  pid_t pid;
  pid_t parent;
  std::string name;
};

// Walk /proc and get information on all the processes running on the system.
static bool GetProcesses(std::vector<Process>* processes) {
  processes->clear();

  DIR* dir = opendir("/proc");
  if (!dir)
    return false;

  struct dirent* dent;
  while ((dent = readdir(dir))) {
    bool candidate = true;

    // Filter out names which aren't ^[0-9]*$
    for (const char* p = dent->d_name; *p; ++p) {
      if (*p < '0' || *p > '9') {
        candidate = false;
        break;
      }
    }

    if (!candidate)
      continue;

    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%s/stat", dent->d_name);
    const int fd = open(buf, O_RDONLY);
    if (fd < 0)
      continue;

    const ssize_t len = HANDLE_EINTR(read(fd, buf, sizeof(buf) - 1));
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close";
    if (len < 1)
      continue;
    buf[len] = 0;

    // The start of the file looks like:
    //   <pid> (<name>) R <parent pid>
    unsigned pid, ppid;
    char *process_name;
    if (sscanf(buf, "%u (%a[^)]) %*c %u", &pid, &process_name, &ppid) != 3)
      continue;

    Process process;
    process.pid = pid;
    process.parent = ppid;
    process.name = process_name;
    free(process_name);
    processes->push_back(process);
  }

  closedir(dir);
  return true;
}

// Given a process name, return the type of the browser which created that
// process, or |MAX_BROWSERS| if we don't know about it.
static BrowserType GetBrowserType(const std::string& process_name) {
  for (unsigned i = 0; kBrowserBinaryNames[i].process_name[0]; ++i) {
    if (strcmp(process_name.c_str(), kBrowserBinaryNames[i].process_name) == 0)
      return kBrowserBinaryNames[i].browser;
  }

  return MAX_BROWSERS;
}

// For each of a list of pids, collect memory information about that process
// and append a record to |out|
static void GetProcessDataMemoryInformation(
    const std::vector<pid_t>& pids, ProcessData* out) {
  for (std::vector<pid_t>::const_iterator
       i = pids.begin(); i != pids.end(); ++i) {
    ProcessMemoryInformation pmi;

    pmi.pid = *i;
    pmi.num_processes = 1;

    if (pmi.pid == base::GetCurrentProcId())
      pmi.type = content::PROCESS_TYPE_BROWSER;
    else
      pmi.type = content::PROCESS_TYPE_UNKNOWN;

    base::ProcessMetrics* metrics =
        base::ProcessMetrics::CreateProcessMetrics(*i);
    metrics->GetWorkingSetKBytes(&pmi.working_set);
    delete metrics;

    out->processes.push_back(pmi);
  }
}

// Find all children of the given process.
static void GetAllChildren(const std::vector<Process>& processes,
                           const pid_t root, const pid_t zygote,
                           std::vector<pid_t>* out) {
  out->clear();
  out->push_back(root);

  std::set<pid_t> wavefront, next_wavefront;
  wavefront.insert(root);
  bool zygote_found = zygote ? false : true;

  while (wavefront.size()) {
    for (std::vector<Process>::const_iterator
         i = processes.begin(); i != processes.end(); ++i) {
      // Handle the zygote separately. With the SUID sandbox and a separate
      // pid namespace, the zygote's parent process is not the browser.
      if (!zygote_found && zygote == i->pid) {
        zygote_found = true;
        out->push_back(i->pid);
        next_wavefront.insert(i->pid);
      } else if (wavefront.count(i->parent)) {
        out->push_back(i->pid);
        next_wavefront.insert(i->pid);
      }
    }

    wavefront.clear();
    wavefront.swap(next_wavefront);
  }
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<Process> processes;
  GetProcesses(&processes);
  std::set<pid_t> browsers_found;

  // For each process on the system, if it appears to be a browser process and
  // it's parent isn't a browser process, then record it in |browsers_found|.
  for (std::vector<Process>::const_iterator
       i = processes.begin(); i != processes.end(); ++i) {
    const BrowserType type = GetBrowserType(i->name);
    if (type != MAX_BROWSERS) {
      bool found_parent = false;

      // Find the parent of |i|
      for (std::vector<Process>::const_iterator
           j = processes.begin(); j != processes.end(); ++j) {
        if (j->pid == i->parent) {
          found_parent = true;

          if (GetBrowserType(j->name) != type) {
            // We went too far and ended up with something else, which means
            // that |i| is a browser.
            browsers_found.insert(i->pid);
            break;
          }
        }
      }

      if (!found_parent)
        browsers_found.insert(i->pid);
    }
  }

  std::vector<pid_t> current_browser_processes;
  const pid_t zygote = ZygoteHost::GetInstance()->pid();
  GetAllChildren(processes, getpid(), zygote, &current_browser_processes);
  ProcessData current_browser;
  GetProcessDataMemoryInformation(current_browser_processes, &current_browser);
  current_browser.name = WideToUTF16(chrome::kBrowserAppName);
  current_browser.process_name = ASCIIToUTF16("chrome");

  for (std::vector<ProcessMemoryInformation>::iterator
       i = current_browser.processes.begin();
       i != current_browser.processes.end(); ++i) {
    // Check if this is one of the child processes whose data we collected
    // on the IO thread, and if so copy over that data.
    for (size_t child = 0; child < child_info.size(); child++) {
      if (child_info[child].pid != i->pid)
        continue;
      i->titles = child_info[child].titles;
      i->type = child_info[child].type;
      break;
    }
  }

  process_data_.push_back(current_browser);

  // For each browser process, collect a list of its children and get the
  // memory usage of each.
  for (std::set<pid_t>::const_iterator
       i = browsers_found.begin(); i != browsers_found.end(); ++i) {
    std::vector<pid_t> browser_processes;
    GetAllChildren(processes, *i, 0, &browser_processes);
    ProcessData browser;
    GetProcessDataMemoryInformation(browser_processes, &browser);

    for (std::vector<Process>::const_iterator
         j = processes.begin(); j != processes.end(); ++j) {
      if (j->pid == *i) {
        BrowserType type = GetBrowserType(j->name);
        if (type != MAX_BROWSERS)
          browser.name = ASCIIToUTF16(kBrowserPrettyNames[type]);
        break;
      }
    }

    process_data_.push_back(browser);
  }

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
