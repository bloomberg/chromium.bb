// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <windows.h>
#include <DbgHelp.h>
#include <string>

#include "chrome_frame/chrome_launcher.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

// TODO(robertshield): Much of the crash report init code is shared with CF and
// probably Chrome too. If this pans out, consider refactoring it into a
// common lib.

namespace {

// Possible names for Pipes:
// Headless (testing) mode: "NamedPipe\ChromeCrashServices"
// System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
// Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

// Assume this implies headless mode and use kChromePipeName if it shows
// up in the command line.
const wchar_t kFullMemoryCrashReport[] = L"full-memory-crash-report";

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

}  // namespace

google_breakpad::CustomClientInfo* GetCustomInfo() {
  // TODO(robertshield): Populate this with actual data.
  std::wstring product(L"ChromeFrame");
  std::wstring version(L"0.1.0.0");
  static google_breakpad::CustomInfoEntry ver_entry(L"ver", version.c_str());
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", product.c_str());
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"chrome_frame");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

google_breakpad::ExceptionHandler* InitializeCrashReporting(
    const wchar_t* cmd_line) {
  if (cmd_line == NULL) {
    return NULL;
  }

  wchar_t temp_path[MAX_PATH + 1] = {0};
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);

  std::wstring pipe_name;
  if (wcsstr(cmd_line, kFullMemoryCrashReport) != NULL) {
    pipe_name = kChromePipeName;
  } else {
    // TODO(robertshield): Figure out if we're a per-user install and connect
    // to the per-user named pipe instead.
    pipe_name = kGoogleUpdatePipeName;
    pipe_name += kSystemPrincipalSid;
  }

  google_breakpad::ExceptionHandler* breakpad =
      new google_breakpad::ExceptionHandler(
          temp_path, NULL, NULL, NULL,
          google_breakpad::ExceptionHandler::HANDLER_ALL, kLargerDumpType,
          pipe_name.c_str(), GetCustomInfo());

  return breakpad;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
  const wchar_t* cmd_line = ::GetCommandLine();

  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(cmd_line));

  UINT exit_code = ERROR_FILE_NOT_FOUND;
  if (chrome_launcher::SanitizeAndLaunchChrome(::GetCommandLine())) {
    exit_code = ERROR_SUCCESS;
  }

  return exit_code;
}
