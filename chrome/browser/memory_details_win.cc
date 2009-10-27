// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"
#include <psapi.h>

#include "app/l10n_util.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"

// Known browsers which we collect details for.
enum {
  CHROME_BROWSER = 0,
  IE_BROWSER,
  FIREFOX_BROWSER,
  OPERA_BROWSER,
  SAFARI_BROWSER,
  IE_64BIT_BROWSER,
  KONQUEROR_BROWSER,
  MAX_BROWSERS
} BrowserProcess;

// Template of static data we use for finding browser process information.
// These entries must match the ordering for MemoryDetails::BrowserProcess.
static ProcessData g_process_template[MAX_BROWSERS];

MemoryDetails::MemoryDetails()
  : ui_loop_(NULL) {
  static const std::wstring google_browser_name =
      l10n_util::GetString(IDS_PRODUCT_NAME);
  ProcessData g_process_template[MAX_BROWSERS] = {
    { google_browser_name.c_str(), L"chrome.exe", },
    { L"IE", L"iexplore.exe", },
    { L"Firefox", L"firefox.exe", },
    { L"Opera", L"opera.exe", },
    { L"Safari", L"safari.exe", },
    { L"IE (64bit)", L"iexplore.exe", },
    { L"Konqueror", L"konqueror.exe", },
  };

  for (int index = 0; index < arraysize(g_process_template); ++index) {
    ProcessData process;
    process.name = g_process_template[index].name;
    process.process_name = g_process_template[index].process_name;
    process_data_.push_back(process);
  }
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[CHROME_BROWSER];
}

void MemoryDetails::CollectProcessData(
    std::vector<ProcessMemoryInformation> child_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Clear old data.
  for (int index = 0; index < arraysize(g_process_template); index++)
    process_data_[index].processes.clear();

  SYSTEM_INFO system_info;
  GetNativeSystemInfo(&system_info);
  bool is_64bit_os =
      system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;

  ScopedHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  PROCESSENTRY32 process_entry = {sizeof(PROCESSENTRY32)};
  if (!snapshot.Get()) {
    LOG(ERROR) << "CreateToolhelp32Snaphot failed: " << GetLastError();
    return;
  }
  if (!::Process32First(snapshot, &process_entry)) {
    LOG(ERROR) << "Process32First failed: " << GetLastError();
    return;
  }
  do {
    base::ProcessId pid = process_entry.th32ProcessID;
    ScopedHandle handle(::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!handle.Get())
      continue;
    bool is_64bit_process = false;
    // IsWow64Process() returns FALSE for a 32bit process on a 32bit OS.
    // We need to check if the real OS is 64bit.
    if (is_64bit_os) {
      BOOL is_wow64 = FALSE;
      // IsWow64Process() is supported by Windows XP SP2 or later.
      IsWow64Process(handle, &is_wow64);
      is_64bit_process = !is_wow64;
    }
    for (int index2 = 0; index2 < arraysize(g_process_template); index2++) {
      if (_wcsicmp(process_data_[index2].process_name.c_str(),
          process_entry.szExeFile) != 0)
        continue;
      if (index2 == IE_BROWSER && is_64bit_process)
        continue;  // Should use IE_64BIT_BROWSER
      // Get Memory Information.
      ProcessMemoryInformation info;
      info.pid = pid;
      if (info.pid == GetCurrentProcessId())
        info.type = ChildProcessInfo::BROWSER_PROCESS;
      else
        info.type = ChildProcessInfo::UNKNOWN_PROCESS;

      scoped_ptr<base::ProcessMetrics> metrics;
      metrics.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
      metrics->GetCommittedKBytes(&info.committed);
      metrics->GetWorkingSetKBytes(&info.working_set);

      // Get Version Information.
      TCHAR name[MAX_PATH];
      if (index2 == CHROME_BROWSER) {
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfoForCurrentModule());
        if (version_info != NULL)
          info.version = version_info->file_version();
        // Check if this is one of the child processes whose data we collected
        // on the IO thread, and if so copy over that data.
        for (size_t child = 0; child < child_info.size(); child++) {
          if (child_info[child].pid != info.pid)
            continue;
          info.titles = child_info[child].titles;
          info.type = child_info[child].type;
          break;
        }
      } else if (GetModuleFileNameEx(handle, NULL, name, MAX_PATH - 1)) {
        std::wstring str_name(name);
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfo(str_name));
        if (version_info != NULL) {
          info.version = version_info->product_version();
          info.product_name = version_info->product_name();
        }
      }

      // Add the process info to our list.
      process_data_[index2].processes.push_back(info);
      break;
    }
  } while (::Process32Next(snapshot, &process_entry));

  // Finally return to the browser thread.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnUIThread));
}
