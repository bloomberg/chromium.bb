// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <psapi.h>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// Known browsers which we collect details for.
enum {
  CHROME_BROWSER = 0,
  CHROME_NACL_PROCESS,
  IE_BROWSER,
  FIREFOX_BROWSER,
  OPERA_BROWSER,
  SAFARI_BROWSER,
  IE_64BIT_BROWSER,
  KONQUEROR_BROWSER,
  MAX_BROWSERS
} BrowserProcess;

MemoryDetails::MemoryDetails() {
  static const std::wstring google_browser_name =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  struct {
    const wchar_t* name;
    const wchar_t* process_name;
  } process_template[MAX_BROWSERS] = {
    { google_browser_name.c_str(), L"chrome.exe", },
    { google_browser_name.c_str(), L"nacl64.exe", },
    { L"IE", L"iexplore.exe", },
    { L"Firefox", L"firefox.exe", },
    { L"Opera", L"opera.exe", },
    { L"Safari", L"safari.exe", },
    { L"IE (64bit)", L"iexplore.exe", },
    { L"Konqueror", L"konqueror.exe", },
  };

  for (int index = 0; index < MAX_BROWSERS; ++index) {
    ProcessData process;
    process.name = process_template[index].name;
    process.process_name = process_template[index].process_name;
    process_data_.push_back(process);
  }
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[CHROME_BROWSER];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Clear old data.
  for (unsigned int index = 0; index < process_data_.size(); index++)
    process_data_[index].processes.clear();

  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->architecture();

  base::win::ScopedHandle snapshot(
      ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
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
    base::win::ScopedHandle process_handle(::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!process_handle.Get())
      continue;
    bool is_64bit_process =
        ((windows_architecture == base::win::OSInfo::X64_ARCHITECTURE) ||
         (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)) &&
        (base::win::OSInfo::GetWOW64StatusForProcess(process_handle) ==
            base::win::OSInfo::WOW64_DISABLED);
    for (unsigned int index2 = 0; index2 < process_data_.size(); index2++) {
      if (_wcsicmp(process_data_[index2].process_name.c_str(),
                   process_entry.szExeFile) != 0)
        continue;
      if (index2 == IE_BROWSER && is_64bit_process)
        continue;  // Should use IE_64BIT_BROWSER
      // Get Memory Information.
      ProcessMemoryInformation info;
      info.pid = pid;
      if (info.pid == GetCurrentProcessId())
        info.type = content::PROCESS_TYPE_BROWSER;
      else
        info.type = content::PROCESS_TYPE_UNKNOWN;

      scoped_ptr<base::ProcessMetrics> metrics;
      metrics.reset(base::ProcessMetrics::CreateProcessMetrics(process_handle));
      metrics->GetCommittedKBytes(&info.committed);
      metrics->GetWorkingSetKBytes(&info.working_set);

      // Get Version Information.
      TCHAR name[MAX_PATH];
      if (index2 == CHROME_BROWSER || index2 == CHROME_NACL_PROCESS) {
        chrome::VersionInfo version_info;
        if (version_info.is_valid())
          info.version = ASCIIToWide(version_info.Version());
        // Check if this is one of the child processes whose data we collected
        // on the IO thread, and if so copy over that data.
        for (size_t child = 0; child < child_info.size(); child++) {
          if (child_info[child].pid != info.pid)
            continue;
          info.titles = child_info[child].titles;
          info.type = child_info[child].type;
          break;
        }
      } else if (GetModuleFileNameEx(process_handle, NULL, name,
                                     MAX_PATH - 1)) {
        std::wstring str_name(name);
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfo(FilePath(str_name)));
        if (version_info != NULL) {
          info.version = version_info->product_version();
          info.product_name = version_info->product_name();
        }
      }

      // Add the process info to our list.
      if (index2 == CHROME_NACL_PROCESS) {
        // Add NaCl processes to Chrome's list
        process_data_[CHROME_BROWSER].processes.push_back(info);
      } else {
        process_data_[index2].processes.push_back(info);
      }
      break;
    }
  } while (::Process32Next(snapshot, &process_entry));

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
