// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/port_monitor/port_monitor.h"

#include <lmcons.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <userenv.h>
#include <windows.h>
#include <winspool.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "cloud_print/virtual_driver/win/port_monitor/spooler_win.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace cloud_print {

const wchar_t kChromeExePath[] = L"google\\chrome\\application\\chrome.exe";
const wchar_t kChromeExePathRegValue[] = L"PathToChromeExe";
const wchar_t kChromeProfilePathRegValue[] = L"PathToChromeProfile";
const bool kIsUnittest = false;

namespace {

// Returns true if Xps support is installed.
bool XpsIsInstalled() {
  base::FilePath xps_path;
  if (!SUCCEEDED(GetPrinterDriverDir(&xps_path))) {
    return false;
  }
  xps_path = xps_path.Append(L"mxdwdrv.dll");
  if (!file_util::PathExists(xps_path)) {
    return false;
  }
  return true;
}

// Returns true if registration/unregistration can be attempted.
bool CanRegister() {
  if (!XpsIsInstalled()) {
    return false;
  }
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    base::IntegrityLevel level = base::INTEGRITY_UNKNOWN;
    if (!GetProcessIntegrityLevel(base::GetCurrentProcessHandle(), &level)) {
      return false;
    }
    if (level != base::HIGH_INTEGRITY) {
      return false;
    }
  }
  return true;
}

}  // namespace


}   // namespace cloud_print

HRESULT WINAPI DllRegisterServer(void) {
  base::AtExitManager at_exit_manager;
  if (!cloud_print::CanRegister()) {
    return E_ACCESSDENIED;
  }
  MONITOR_INFO_2 monitor_info = {0};
  // YUCK!!!  I can either copy the constant, const_cast, or define my own
  // MONITOR_INFO_2 that will take const strings.
  base::FilePath dll_path(cloud_print::GetPortMonitorDllName());
  monitor_info.pDLLName = const_cast<LPWSTR>(dll_path.value().c_str());
  monitor_info.pName = const_cast<LPWSTR>(dll_path.value().c_str());
  if (AddMonitor(NULL, 2, reinterpret_cast<BYTE*>(&monitor_info))) {
    return S_OK;
  }
  return cloud_print::GetLastHResult();
}

HRESULT WINAPI DllUnregisterServer(void) {
  base::AtExitManager at_exit_manager;
  if (!cloud_print::CanRegister()) {
    return E_ACCESSDENIED;
  }
  base::FilePath dll_path(cloud_print::GetPortMonitorDllName());
  if (DeleteMonitor(NULL,
                    NULL,
                    const_cast<LPWSTR>(dll_path.value().c_str()))) {
    return S_OK;
  }
  return cloud_print::GetLastHResult();
}
