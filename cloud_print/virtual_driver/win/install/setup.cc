// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace {

bool IsSystem64Bit() {
  base::win::OSInfo::WindowsArchitecture arch =
      base::win::OSInfo::GetInstance()->architecture();
  return (arch == base::win::OSInfo::X64_ARCHITECTURE) ||
         (arch == base::win::OSInfo::IA64_ARCHITECTURE);
}

const wchar_t *GetPortMonitorDllName() {
  if (IsSystem64Bit()) {
    return cloud_print::kPortMonitorDllName64;
  } else {
    return cloud_print::kPortMonitorDllName32;
  }
}

HRESULT GetPortMonitorDllPath(FilePath* path) {
  if (!PathService::Get(base::DIR_EXE, path)) {
    return ERROR_PATH_NOT_FOUND;
  }
  *path = path->Append(GetPortMonitorDllName());
  return S_OK;
}

HRESULT GetPortMonitorInstallPath(FilePath* path) {
  if (IsSystem64Bit()) {
    if (!PathService::Get(base::DIR_WINDOWS, path)) {
      return ERROR_PATH_NOT_FOUND;
    }
    // Sysnative will bypass filesystem redirection and give us
    // the location of the 64bit system32 from a 32 bit process.
    *path = path->Append(L"sysnative");
  } else {
    if (!PathService::Get(base::DIR_SYSTEM, path)) {
      return ERROR_PATH_NOT_FOUND;
    }
  }
  *path = path->Append(GetPortMonitorDllName());
  return S_OK;
}

HRESULT GetRegsvr32Path(FilePath* path) {
  if (!PathService::Get(base::DIR_SYSTEM, path)) {
    return ERROR_PATH_NOT_FOUND;
  }
  *path = path->Append(FilePath(L"regsvr32.exe"));
  return S_OK;
}

HRESULT RegisterPortMonitor(bool install) {
  FilePath target_path;
  HRESULT result = S_OK;
  result = GetPortMonitorInstallPath(&target_path);
  if (!SUCCEEDED(result)) {
    return result;
  }
  FilePath dll_path;
  result = GetPortMonitorDllPath(&dll_path);
  if (!SUCCEEDED(result)) {
    return result;
  }
  if (install) {
    if (!file_util::CopyFileW(dll_path, target_path)) {
      return cloud_print::GetLastHResult();
    }
  }
  FilePath regsvr32_path;
  result = GetRegsvr32Path(&regsvr32_path);
  if (!SUCCEEDED(result)) {
    return result;
  }
  CommandLine command_line(regsvr32_path);
  command_line.AppendArg("/s");
  if (!install) {
    command_line.AppendArg("/u");
  }
  command_line.AppendArgPath(dll_path);
  HANDLE process_handle;
  if (!base::LaunchApp(command_line.command_line_string(),
                       true,
                       false,
                       &process_handle)) {
    return cloud_print::GetLastHResult();
  }
  base::win::ScopedHandle scoped_process_handle(process_handle);
  DWORD exit_code = S_OK;
  if (!GetExitCodeProcess(scoped_process_handle, &exit_code)) {
    return cloud_print::GetLastHResult();
  }
  if (exit_code != 0) {
    return  HRESULT_FROM_WIN32(exit_code);
  }
  if (!install) {
    if (!file_util::Delete(target_path, false)) {
      return cloud_print::GetLastHResult();
    }
  }
  return S_OK;
}

HRESULT InstallVirtualDriver(void) {
  return RegisterPortMonitor(true);
}

HRESULT UninstallVirtualDriver(void) {
  return RegisterPortMonitor(false);
}

}  // namespace

int WINAPI WinMain(__in  HINSTANCE hInstance,
            __in  HINSTANCE hPrevInstance,
            __in  LPSTR lpCmdLine,
            __in  int nCmdShow) {
  base::AtExitManager at_exit_manager;
  CommandLine::Init(0, NULL);
  HRESULT retval = S_OK;
  if (CommandLine::ForCurrentProcess()->HasSwitch("uninstall")) {
    retval = UninstallVirtualDriver();
  } else {
    retval = InstallVirtualDriver();
  }
  if (!CommandLine::ForCurrentProcess()->HasSwitch("silent")) {
    cloud_print::DisplayWindowsMessage(NULL, retval);
  }
  return retval;
}

