// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/mac/mac_util.h"
#include "base/string_util.h"
#include "base/process_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/process_info_snapshot.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(viettrungluu): Many of the TODOs below are subsumed by a general need to
// refactor the about:memory code (not just on Mac, but probably on other
// platforms as well). I've filed crbug.com/25456.

class RenderViewHostDelegate;

// Known browsers which we collect details for. |CHROME_BROWSER| *must* be the
// first browser listed. The order here must match those in |process_template|
// (in |MemoryDetails::MemoryDetails()| below).
// TODO(viettrungluu): In the big refactoring (see above), get rid of this order
// dependence.
enum BrowserType {
  // TODO(viettrungluu): possibly add more?
  CHROME_BROWSER = 0,
  SAFARI_BROWSER,
  FIREFOX_BROWSER,
  CAMINO_BROWSER,
  OPERA_BROWSER,
  OMNIWEB_BROWSER,
  MAX_BROWSERS
} BrowserProcess;


MemoryDetails::MemoryDetails() {
  static const std::string google_browser_name =
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  // (Human and process) names of browsers; should match the ordering for
  // |BrowserProcess| (i.e., |BrowserType|).
  // TODO(viettrungluu): The current setup means that we can't detect both
  // Chrome and Chromium at the same time!
  // TODO(viettrungluu): Get localized browser names for other browsers
  // (crbug.com/25779).
  struct {
    const char* name;
    const char* process_name;
  } process_template[MAX_BROWSERS] = {
    { google_browser_name.c_str(), chrome::kBrowserProcessExecutableName, },
    { "Safari", "Safari", },
    { "Firefox", "firefox-bin", },
    { "Camino", "Camino", },
    { "Opera", "Opera", },
    { "OmniWeb", "OmniWeb", },
  };

  for (size_t index = 0; index < MAX_BROWSERS; ++index) {
    ProcessData process;
    process.name = UTF8ToUTF16(process_template[index].name);
    process.process_name = UTF8ToUTF16(process_template[index].process_name);
    process_data_.push_back(process);
  }
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[CHROME_BROWSER];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  // This must be run on the file thread to avoid jank (|ProcessInfoSnapshot|
  // runs /bin/ps, which isn't instantaneous).
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Clear old data.
  for (size_t index = 0; index < MAX_BROWSERS; index++)
    process_data_[index].processes.clear();

  // First, we use |NamedProcessIterator| to get the PIDs of the processes we're
  // interested in; we save our results to avoid extra calls to
  // |NamedProcessIterator| (for performance reasons) and to avoid additional
  // inconsistencies caused by racing. Then we run |/bin/ps| *once* to get
  // information on those PIDs. Then we used our saved information to iterate
  // over browsers, then over PIDs.

  // Get PIDs of main browser processes.
  std::vector<base::ProcessId> pids_by_browser[MAX_BROWSERS];
  std::vector<base::ProcessId> all_pids;
  for (size_t index = CHROME_BROWSER; index < MAX_BROWSERS; index++) {
    base::NamedProcessIterator process_it(
        UTF16ToUTF8(process_data_[index].process_name), NULL);

    while (const base::ProcessEntry* entry = process_it.NextProcessEntry()) {
      pids_by_browser[index].push_back(entry->pid());
      all_pids.push_back(entry->pid());
    }
  }

  // Get PIDs of helpers.
  std::vector<base::ProcessId> helper_pids;
  {
    base::NamedProcessIterator helper_it(chrome::kHelperProcessExecutableName,
                                         NULL);
    while (const base::ProcessEntry* entry = helper_it.NextProcessEntry()) {
      helper_pids.push_back(entry->pid());
      all_pids.push_back(entry->pid());
    }
  }

  // Capture information about the processes we're interested in.
  ProcessInfoSnapshot process_info;
  process_info.Sample(all_pids);

  // Handle the other processes first.
  for (size_t index = CHROME_BROWSER + 1; index < MAX_BROWSERS; index++) {
    for (std::vector<base::ProcessId>::const_iterator it =
         pids_by_browser[index].begin();
         it != pids_by_browser[index].end(); ++it) {
      ProcessMemoryInformation info;
      info.pid = *it;
      info.type = ChildProcessInfo::UNKNOWN_PROCESS;

      // Try to get version information. To do this, we need first to get the
      // executable's name (we can only believe |proc_info.command| if it looks
      // like an absolute path). Then we need strip the executable's name back
      // to the bundle's name. And only then can we try to get the version.
      scoped_ptr<FileVersionInfo> version_info;
      ProcessInfoSnapshot::ProcInfoEntry proc_info;
      if (process_info.GetProcInfo(info.pid, &proc_info)) {
        if (proc_info.command.length() > 1 && proc_info.command[0] == '/') {
          FilePath bundle_name =
              base::mac::GetAppBundlePath(FilePath(proc_info.command));
          if (!bundle_name.empty()) {
            version_info.reset(FileVersionInfo::CreateFileVersionInfo(
                bundle_name));
          }
        }
      }
      if (version_info.get()) {
        info.product_name = version_info->product_name();
        info.version = version_info->product_version();
      } else {
        info.product_name = process_data_[index].name;
        info.version = string16();
      }

      // Memory info.
      process_info.GetCommittedKBytesOfPID(info.pid, &info.committed);
      process_info.GetWorkingSetKBytesOfPID(info.pid, &info.working_set);

      // Add the process info to our list.
      process_data_[index].processes.push_back(info);
    }
  }

  // Collect data about Chrome/Chromium.
  for (std::vector<base::ProcessId>::const_iterator it =
       pids_by_browser[CHROME_BROWSER].begin();
       it != pids_by_browser[CHROME_BROWSER].end(); ++it) {
    CollectProcessDataChrome(child_info, *it, process_info);
  }

  // And collect data about the helpers.
  for (std::vector<base::ProcessId>::const_iterator it = helper_pids.begin();
       it != helper_pids.end(); ++it) {
    CollectProcessDataChrome(child_info, *it, process_info);
  }

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnUIThread));
}

void MemoryDetails::CollectProcessDataChrome(
    const std::vector<ProcessMemoryInformation>& child_info,
    base::ProcessId pid,
    const ProcessInfoSnapshot& process_info) {
  ProcessMemoryInformation info;
  info.pid = pid;
  if (info.pid == base::GetCurrentProcId())
    info.type = ChildProcessInfo::BROWSER_PROCESS;
  else
    info.type = ChildProcessInfo::UNKNOWN_PROCESS;

  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    info.product_name = ASCIIToUTF16(version_info.Name());
    info.version = ASCIIToUTF16(version_info.Version());
  } else {
    info.product_name = process_data_[CHROME_BROWSER].name;
    info.version = string16();
  }

  // Check if this is one of the child processes whose data we collected
  // on the IO thread, and if so copy over that data.
  for (size_t child = 0; child < child_info.size(); child++) {
    if (child_info[child].pid == info.pid) {
      info.titles = child_info[child].titles;
      info.type = child_info[child].type;
      break;
    }
  }

  // Memory info.
  process_info.GetCommittedKBytesOfPID(info.pid, &info.committed);
  process_info.GetWorkingSetKBytesOfPID(info.pid, &info.working_set);

  // Add the process info to our list.
  process_data_[CHROME_BROWSER].processes.push_back(info);
}
