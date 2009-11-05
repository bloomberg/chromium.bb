// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/mem_usage.h"

#include <windows.h>
#include <psapi.h>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/chrome_process_util.h"

// GetPerformanceInfo is not available on WIN2K.  So we'll
// load it on-the-fly.
const wchar_t kPsapiDllName[] = L"psapi.dll";
typedef BOOL (WINAPI *GetPerformanceInfoFunction) (
    PPERFORMANCE_INFORMATION pPerformanceInformation,
    DWORD cb);

static BOOL InternalGetPerformanceInfo(
    PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb) {
  static GetPerformanceInfoFunction GetPerformanceInfo_func = NULL;
  if (!GetPerformanceInfo_func) {
    HMODULE psapi_dll = ::GetModuleHandle(kPsapiDllName);
    if (psapi_dll)
      GetPerformanceInfo_func = reinterpret_cast<GetPerformanceInfoFunction>(
          GetProcAddress(psapi_dll, "GetPerformanceInfo"));

    if (!GetPerformanceInfo_func) {
      // The function could be loaded!
      memset(pPerformanceInformation, 0, cb);
      return FALSE;
    }
  }
  return GetPerformanceInfo_func(pPerformanceInformation, cb);
}


size_t GetSystemCommitCharge() {
  // Get the System Page Size.
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  PERFORMANCE_INFORMATION info;
  if (InternalGetPerformanceInfo(&info, sizeof(info)))
    return info.CommitTotal * system_info.dwPageSize;
  return -1;
}

void PrintChromeMemoryUsageInfo() {
  printf("\n");

  FilePath data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &data_dir);
  int browser_process_pid = ChromeBrowserProcessId(data_dir);
  ChromeProcessList chrome_processes(GetRunningChromeProcesses(data_dir));

  ChromeProcessList::const_iterator it;
  for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
    base::ProcessHandle process_handle;
    if (!base::OpenPrivilegedProcessHandle(*it, &process_handle)) {
      NOTREACHED();
    }

    // TODO(sgk):  if/when base::ProcessMetrics can return real memory
    // stats on mac, convert to:
    //
    // scoped_ptr<base::ProcessMetrics> process_metrics;
    // process_metrics.reset(
    //     base::ProcessMetrics::CreateProcessMetrics(process_handle));
    scoped_ptr<ChromeTestProcessMetrics> process_metrics;
    process_metrics.reset(
        ChromeTestProcessMetrics::CreateProcessMetrics(process_handle));

    size_t peak_virtual_size = process_metrics->GetPeakPagefileUsage();
    size_t current_virtual_size = process_metrics->GetPagefileUsage();
    size_t peak_working_set_size = process_metrics->GetPeakWorkingSetSize();
    size_t current_working_set_size = process_metrics->GetWorkingSetSize();

    if (*it == browser_process_pid) {
      wprintf(L"browser_vm_peak = %d\n", peak_virtual_size);
      wprintf(L"browser_vm_current = %d\n", current_virtual_size);
      wprintf(L"browser_ws_peak = %d\n", peak_working_set_size);
      wprintf(L"browser_ws_final = %d\n", current_working_set_size);
    } else {
      wprintf(L"render_vm_peak = %d\n", peak_virtual_size);
      wprintf(L"render_vm_current = %d\n", current_virtual_size);
      wprintf(L"render_ws_peak = %d\n", peak_working_set_size);
      wprintf(L"render_ws_final = %d\n", current_working_set_size);
    }
  };
}
