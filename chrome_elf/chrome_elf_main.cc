// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_main.h"

#include <windows.h>
#include <algorithm>

#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "base/win/iat_patch_function.h"
#include "build/build_config.h"
#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/blacklist/crashpad_helper.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/chrome_elf_security.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_keys.h"

namespace {

base::LazyInstance<std::vector<crash_reporter::Report>>::Leaky g_crash_reports =
    LAZY_INSTANCE_INITIALIZER;

// Gets the exe name from the full path of the exe.
base::string16 GetExeName() {
  wchar_t file_path[MAX_PATH] = {};
  if (!::GetModuleFileName(nullptr, file_path, arraysize(file_path))) {
    assert(false);
    return base::string16();
  }
  base::string16 file_name_string = file_path;
  size_t last_slash_pos = file_name_string.find_last_of(L'\\');
  if (last_slash_pos != base::string16::npos) {
    file_name_string = file_name_string.substr(
        last_slash_pos + 1, file_name_string.length() - last_slash_pos);
  }
  std::transform(file_name_string.begin(), file_name_string.end(),
                 file_name_string.begin(), ::tolower);
  return file_name_string;
}

void InitializeCrashReportingForProcess() {
  // We want to initialize crash reporting only in chrome.exe
  if (GetExeName() != L"chrome.exe")
    return;
  ChromeCrashReporterClient::InitializeCrashReportingForProcess();
}

#if !defined(ADDRESS_SANITIZER)
// chrome_elf loads early in the process and initializes Crashpad. That in turn
// uses the SetUnhandledExceptionFilter API to set a top level exception
// handler for the process. When the process eventually initializes, CRT sets
// an exception handler which calls TerminateProcess which effectively bypasses
// us. Ideally we want to be at the top of the unhandled exception filter
// chain. However we don't have a good way of intercepting the
// SetUnhandledExceptionFilter API in the sandbox. EAT patching kernel32 or
// kernelbase should ideally work. However the kernel32 kernelbase dlls are
// prebound which causes EAT patching to not work. Sidestep works. However it
// is only supported for 32 bit. For now we use IAT patching for the
// executable.
// TODO(ananta).
// Check if it is possible to fix EAT patching or use sidestep patching for
// 32 bit and 64 bit for this purpose.
base::win::IATPatchFunction g_set_unhandled_exception_filter;

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI
SetUnhandledExceptionFilterPatch(LPTOP_LEVEL_EXCEPTION_FILTER filter) {
  // Don't set the exception filter. Please see above for comments.
  return nullptr;
}

// Please refer above to more information about why we intercept the
// SetUnhandledExceptionFilter API.
void DisableSetUnhandledExceptionFilter() {
  DWORD patched = g_set_unhandled_exception_filter.PatchFromModule(
      GetModuleHandle(nullptr), "kernel32.dll", "SetUnhandledExceptionFilter",
      SetUnhandledExceptionFilterPatch);
  CHECK(patched == 0);
}
#endif // !defined(ADDRESS_SANITIZER)

}  // namespace

void SignalChromeElf() {
  blacklist::ResetBeacon();
}

// This helper is invoked by code in chrome.dll to retrieve the crash reports.
// See CrashUploadListCrashpad. Note that we do not pass an std::vector here,
// because we do not want to allocate/free in different modules. The returned
// pointer is read-only.
extern "C" __declspec(dllexport) void GetCrashReportsImpl(
    const crash_reporter::Report** reports,
    size_t* report_count) {
  crash_reporter::GetReports(g_crash_reports.Pointer());
  *reports = g_crash_reports.Pointer()->data();
  *report_count = g_crash_reports.Pointer()->size();
}

// This helper is invoked by debugging code in chrome to register the client
// id.
extern "C" __declspec(dllexport) void SetMetricsClientId(
    const char* client_id) {
  if (client_id)
    crash_keys::SetMetricsClientIdFromGUID(client_id);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    InitializeCrashReportingForProcess();
    // CRT on initialization installs an exception filter which calls
    // TerminateProcess. We need to hook CRT's attempt to set an exception
    // handler and ignore it. Don't do this when ASan is present, or ASan will
    // fail to install its own unhandled exception filter.
#if !defined(ADDRESS_SANITIZER)
    DisableSetUnhandledExceptionFilter();
#endif

    install_static::InitializeProcessType();
    if (install_static::g_process_type ==
        install_static::ProcessType::BROWSER_PROCESS)
      EarlyBrowserSecurity();

    __try {
      blacklist::Initialize(false);  // Don't force, abort if beacon is present.
    } __except(GenerateCrashDump(GetExceptionInformation())) {
    }
  }
  return TRUE;
}
