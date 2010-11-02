// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the launch with gflags function.
#include "ceee/testing/utils/gflag_utils.h"

#include "ceee/testing/utils/nt_internals.h"
#include <shlwapi.h>

namespace testing {

using nt_internals::NTSTATUS;
using nt_internals::NtQueryInformationProcess;

HRESULT WriteProcessGFlags(HANDLE process, DWORD gflags) {
  nt_internals::PROCESS_BASIC_INFORMATION basic_info = {};
  ULONG return_len = 0;
  NTSTATUS status =
      NtQueryInformationProcess(process,
                                nt_internals::ProcessBasicInformation,
                                &basic_info,
                                sizeof(basic_info),
                                &return_len);
  if (0 != status)
    return HRESULT_FROM_NT(status);
  else if (return_len != sizeof(basic_info))
    return E_UNEXPECTED;

  LPVOID gflags_location =
      reinterpret_cast<char*>(basic_info.PebBaseAddress) + kGFlagsPEBOffset;

  SIZE_T num_written = 0;
  if (!::WriteProcessMemory(process,
                            gflags_location,
                            &gflags,
                            sizeof(gflags),
                            &num_written) ||
      sizeof(gflags) != num_written)
    return HRESULT_FROM_WIN32(::GetLastError());

  return S_OK;
}

HRESULT CreateProcessWithGFlags(LPCWSTR application_name,
                                LPWSTR command_line,
                                LPSECURITY_ATTRIBUTES process_attributes,
                                LPSECURITY_ATTRIBUTES thread_attributes,
                                BOOL inherit_handles,
                                DWORD creation_flags,
                                LPVOID environment,
                                LPCWSTR current_directory,
                                LPSTARTUPINFOW startup_info,
                                LPPROCESS_INFORMATION proc_info,
                                DWORD gflags) {
  // Create the process, but make sure it's suspended at creation.
  BOOL created = ::CreateProcess(application_name,
                                 command_line,
                                 process_attributes,
                                 thread_attributes,
                                 inherit_handles,
                                 creation_flags | CREATE_SUSPENDED,
                                 environment,
                                 current_directory,
                                 startup_info,
                                 proc_info);
  if (!created)
    return HRESULT_FROM_WIN32(::GetLastError());

  // If we succeeded, go through and write the new process'
  // PEB with the requested gflags.
  HRESULT hr = WriteProcessGFlags(proc_info->hProcess, gflags);
  if (FAILED(hr)) {
    // We failed to write the gflags value, clean up and bail.
    ::TerminateProcess(proc_info->hProcess, 0);
    ::CloseHandle(proc_info->hProcess);
    ::CloseHandle(proc_info->hThread);

    return hr;
  }

  // Resume the main thread unless suspended creation was requested.
  if (!(creation_flags & CREATE_SUSPENDED))
    ::ResumeThread(proc_info->hThread);

  return hr;
}

HRESULT RelaunchWithGFlags(DWORD wanted_gflags) {
  DWORD current_gflags = nt_internals::RtlGetNtGlobalFlags();

  // Do we already have all the wanted_gflags?
  if (wanted_gflags == (wanted_gflags & current_gflags))
    return S_FALSE;

  wchar_t app_name[MAX_PATH] = {};
  if (!::GetModuleFileName(NULL, app_name, ARRAYSIZE(app_name)))
    return HRESULT_FROM_WIN32(::GetLastError());

  // If there is a GFlags registry entry for our executable, we don't
  // want to relaunch because it's futile. The spawned process would pick
  // up the registry settings in preference to ours, and we'd effectively
  // have a fork bomb on our hands.
  wchar_t key_name[MAX_PATH] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\"
      L"Image File Execution Options\\";

  if (0 != wcscat_s(key_name, ::PathFindFileName(app_name)))
    return E_UNEXPECTED;

  HKEY key = NULL;
  LONG err = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, key_name, 0, KEY_READ, &key);
  if (err == ERROR_SUCCESS) {
    // There's a setting for this executable in registry, does it
    // specify global flags?
    DWORD data_len = 0;
    err = ::RegQueryValueEx(key, L"GlobalFlag", NULL, NULL, NULL, &data_len);

    // Close the key.
    ::RegCloseKey(key);

    if (err == ERROR_SUCCESS) {
      // We can't relaunch because we'd run the risk of a fork bomb.
      return E_FAIL;
    }
  }

  wchar_t current_dir[MAX_PATH] = {};
  if (!::GetCurrentDirectory(ARRAYSIZE(current_dir), current_dir))
    return HRESULT_FROM_WIN32(::GetLastError());

  STARTUPINFO startup_info = {};
  ::GetStartupInfo(&startup_info);
  PROCESS_INFORMATION proc_info = {};
  HRESULT hr;
  hr = CreateProcessWithGFlags(app_name,
                               ::GetCommandLine(),
                               NULL,
                               NULL,
                               FALSE,
                               0,
                               NULL,
                               current_dir,
                               &startup_info,
                               &proc_info,
                               current_gflags | wanted_gflags);
  if (SUCCEEDED(hr)) {
    ::WaitForSingleObject(proc_info.hProcess, INFINITE);
    DWORD exit_code = 0;
    ::GetExitCodeProcess(proc_info.hProcess, &exit_code);
    ::ExitProcess(exit_code);
  }

  return hr;
}

}  //  namespace testing
