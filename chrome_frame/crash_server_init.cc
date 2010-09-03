// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/crash_server_init.h"

#include "version.h"  // NOLINT

const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

const wchar_t kFullMemoryCrashReport[] = L"full-memory-crash-report";

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

google_breakpad::CustomClientInfo* GetCustomInfo() {
  static google_breakpad::CustomInfoEntry ver_entry(
      L"ver", TEXT(CHROME_VERSION_STRING));
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"ChromeFrame");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"chrome_frame");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, ARRAYSIZE(entries) };
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
